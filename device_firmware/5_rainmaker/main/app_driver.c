#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "iot_button.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include <esp_rmaker_utils.h>

#include DEVELOPMENT_BOARD
#include "app_priv.h"
#include "esp_rmaker_core.h"

extern esp_rmaker_param_t *dimming_param;
extern esp_rmaker_param_t *blink_param;
extern esp_rmaker_param_t *meteor_param;
extern esp_rmaker_param_t *rainbow_param;
extern esp_rmaker_param_t *effect_param;

#define TAG "app_driver"
#define REBOOT_DELAY 2
#define DIM_STEP 1
#define DIM_TICK_MS 30
#define DIM_MIN_VALUE 5
#define DIM_MAX_VALUE 100
#define RAINBOW_TICK_MS 50
#define RAINBOW_HUE_STEP 3
#define BLINK_TICK_MS 500
#define METEOR_TICK_MS 100
#define METEOR_TRAIL_LENGTH 4

static bool g_output_state = true;
static uint16_t g_brightness = 100;
static uint16_t g_hue = 0;
static uint8_t g_saturation = 0;

static bool g_is_dimming = false;
static TimerHandle_t g_dim_timer = NULL;
static int8_t g_dim_direction = -1;

static bool g_is_rainbow = false;
static TimerHandle_t g_rainbow_timer = NULL;
static uint16_t g_rainbow_hue = 0;

static bool g_is_blinking = false;
static bool g_blink_visible = true;
static TimerHandle_t g_blink_timer = NULL;

static bool g_is_meteor = false;
static TimerHandle_t g_meteor_timer = NULL;
static uint16_t g_meteor_head = 0;

static led_strip_handle_t g_led_strip = NULL;
static SemaphoreHandle_t g_render_mutex = NULL;

static void push_btn_cb(void *arg);
static void factory_reset_trigger(void *arg);
static void dim_timer_cb(TimerHandle_t xTimer);
static void blink_timer_cb(TimerHandle_t xTimer);
static void meteor_timer_cb(TimerHandle_t xTimer);
static void rainbow_timer_cb(TimerHandle_t xTimer);
static void stop_blink_if_running(void);
static void stop_meteor_if_running(void);
static void stop_rainbow_if_running(void);
static void dimmer_btn_cb(void *arg);
static void report_bool(esp_rmaker_param_t *param, bool value);
static void sync_effect_param(void);
static esp_err_t render_led_unlocked(void);
static esp_err_t render_led(void);
static void hsv_to_rgb(uint16_t hue, uint8_t saturation, uint16_t value,
                       uint8_t *red, uint8_t *green, uint8_t *blue);

static void push_btn_cb(void *arg)
{
    app_driver_set_state(!g_output_state);
}

static void factory_reset_trigger(void *arg)
{
    esp_rmaker_factory_reset(0, REBOOT_DELAY);
}

/* Report a bool param to the RainMaker cloud so the phone app stays in sync. */
static void report_bool(esp_rmaker_param_t *param, bool value)
{
    if (param != NULL)
    {
        esp_rmaker_param_update_and_report(param, esp_rmaker_bool(value));
    }
}

/* Keep the app's Effect dropdown in sync with the active physical effect. */
static void sync_effect_param(void)
{
    if (effect_param == NULL)
        return;

    const char *effect = APP_LIGHT_EFFECT_STATIC;
    if (g_is_dimming)
        effect = APP_LIGHT_EFFECT_BREATHING;
    else if (g_is_blinking)
        effect = APP_LIGHT_EFFECT_BLINK;
    else if (g_is_rainbow)
        effect = APP_LIGHT_EFFECT_RAINBOW_WAVE;
    else if (g_is_meteor)
        effect = APP_LIGHT_EFFECT_METEOR;

    esp_rmaker_param_update_and_report(effect_param, esp_rmaker_str(effect));
}

/* Convert HSV (H: 0..360, S/V: 0..100) to RGB (0..255). */
static void hsv_to_rgb(uint16_t hue, uint8_t saturation, uint16_t value,
                       uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (hue >= 360)
        hue %= 360;
    if (saturation > 100)
        saturation = 100;
    if (value > 100)
        value = 100;

    uint16_t v = (uint16_t)(value * 255U / 100U);
    uint16_t c = (uint16_t)(v * saturation / 100U);
    uint16_t hmod = hue % 120U;
    uint16_t distance = (hmod > 60U) ? (hmod - 60U) : (60U - hmod);
    uint16_t x = (uint16_t)(c * (60U - distance) / 60U);
    uint16_t m = v - c;
    uint16_t r1 = 0, g1 = 0, b1 = 0;

    switch (hue / 60U)
    {
    case 0:
        r1 = c;
        g1 = x;
        break;
    case 1:
        r1 = x;
        g1 = c;
        break;
    case 2:
        g1 = c;
        b1 = x;
        break;
    case 3:
        g1 = x;
        b1 = c;
        break;
    case 4:
        r1 = x;
        b1 = c;
        break;
    default:
        r1 = c;
        b1 = x;
        break;
    }

    *red = (uint8_t)(r1 + m);
    *green = (uint8_t)(g1 + m);
    *blue = (uint8_t)(b1 + m);
}

/* Set all pixels: a single color, or a rotating rainbow when g_is_rainbow is set. */
static esp_err_t render_led_unlocked(void)
{
    if (g_led_strip == NULL)
        return ESP_ERR_INVALID_STATE;

    uint8_t red = 0, green = 0, blue = 0;
    if (g_output_state && (!g_is_blinking || g_blink_visible))
    {
        hsv_to_rgb(g_hue, g_saturation, g_brightness, &red, &green, &blue);
    }

    for (int i = 0; i < LIGHT_STRIP_LED_COUNT; i++)
    {
        uint8_t pixel_red = red;
        uint8_t pixel_green = green;
        uint8_t pixel_blue = blue;

        if (g_output_state && g_is_rainbow)
        {
            /* Spread the hue evenly across the ring. */
            uint16_t pixel_hue = (uint16_t)((g_rainbow_hue +
                                             ((uint32_t)i * 360U / LIGHT_STRIP_LED_COUNT)) %
                                            360U);

            hsv_to_rgb(pixel_hue, 100, g_brightness,
                       &pixel_red, &pixel_green, &pixel_blue);
        }
        else if (g_output_state && g_is_meteor)
        {
            uint16_t trail_distance =
                (uint16_t)((g_meteor_head + LIGHT_STRIP_LED_COUNT - i) %
                           LIGHT_STRIP_LED_COUNT);

            if (trail_distance < METEOR_TRAIL_LENGTH)
            {
                uint16_t trail_brightness =
                    (uint16_t)(g_brightness *
                               (METEOR_TRAIL_LENGTH - trail_distance) /
                               METEOR_TRAIL_LENGTH);
                hsv_to_rgb(g_hue, g_saturation, trail_brightness,
                           &pixel_red, &pixel_green, &pixel_blue);
            }
            else
            {
                pixel_red = 0;
                pixel_green = 0;
                pixel_blue = 0;
            }
        }

        esp_err_t err = led_strip_set_pixel(g_led_strip, i,
                                            pixel_red, pixel_green, pixel_blue);
        if (err != ESP_OK)
            return err;
    }
    return led_strip_refresh(g_led_strip);
}

/* render_led() may be called from the timer, button and RainMaker tasks, so serialize it. */
static esp_err_t render_led(void)
{
    if (g_render_mutex == NULL)
        return render_led_unlocked();

    xSemaphoreTake(g_render_mutex, portMAX_DELAY);
    esp_err_t err = render_led_unlocked();
    xSemaphoreGive(g_render_mutex);
    return err;
}

static void dim_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    /* A tick may still be queued after xTimerStop(), so re-check the flag. */
    if (!g_is_dimming || !g_output_state)
        return;

    int16_t new_value = (int16_t)g_brightness + g_dim_direction * DIM_STEP;
    if (new_value >= DIM_MAX_VALUE)
    {
        new_value = DIM_MAX_VALUE;
        g_dim_direction = -1;
    }
    else if (new_value <= DIM_MIN_VALUE)
    {
        new_value = DIM_MIN_VALUE;
        g_dim_direction = 1;
    }
    g_brightness = (uint16_t)new_value;
    (void)render_led();
}

static void rainbow_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!g_is_rainbow || !g_output_state)
        return;

    g_rainbow_hue =
        (uint16_t)((g_rainbow_hue + RAINBOW_HUE_STEP) % 360U);

    (void)render_led();
}

static void blink_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!g_is_blinking || !g_output_state)
        return;

    g_blink_visible = !g_blink_visible;
    (void)render_led();
}

static void meteor_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (!g_is_meteor || !g_output_state)
        return;

    g_meteor_head = (g_meteor_head + 1U) % LIGHT_STRIP_LED_COUNT;
    (void)render_led();
}

static void stop_blink_if_running(void)
{
    if (g_is_blinking)
    {
        g_is_blinking = false;
        g_blink_visible = true;
        if (g_blink_timer != NULL)
            (void)xTimerStop(g_blink_timer, 0);
        report_bool(blink_param, false);
        sync_effect_param();
        ESP_LOGI(TAG, "Blink: OFF");
    }
}

static void stop_meteor_if_running(void)
{
    if (g_is_meteor)
    {
        g_is_meteor = false;
        if (g_meteor_timer != NULL)
            (void)xTimerStop(g_meteor_timer, 0);
        report_bool(meteor_param, false);
        sync_effect_param();
        ESP_LOGI(TAG, "Meteor: OFF");
    }
}

static void stop_rainbow_if_running(void)
{
    if (g_is_rainbow)
    {
        g_is_rainbow = false;
        if (g_rainbow_timer != NULL)
            (void)xTimerStop(g_rainbow_timer, 0);
        report_bool(rainbow_param, false);
        sync_effect_param();
        ESP_LOGI(TAG, "Rainbow: OFF");
    }
}

/*
 * Dimmer button cycle:
 *   Static -> Breathing (Dimming) -> Blink -> Rainbow Wave -> Meteor -> Static
 * The next state is derived from the current state, so it stays in sync
 * even when the mode is changed from the RainMaker app.
 * Param reporting is done inside the effect setters.
 */
static void dimmer_btn_cb(void *arg)
{
    (void)arg;


    if (g_is_dimming)
    {
        (void)app_light_set_blink(true);
    }
    else if (g_is_blinking)
    {
        (void)app_light_set_rainbow(true);
    }
    else if (g_is_rainbow)
    {
        (void)app_light_set_meteor(true);
    }
    else if (g_is_meteor)
    {
        (void)app_light_set_meteor(false);
    }
    else
    {
        (void)app_light_set_dimming(true);
    }
}

void app_driver_init(void)
{
    g_render_mutex = xSemaphoreCreateMutex();
    if (g_render_mutex == NULL)
    {
        ESP_LOGE(TAG, "Could not create render mutex");
    }

    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = LIGHT_BUTTON_GPIO,
            .active_level = LIGHT_BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn_handle = iot_button_create(&btn_cfg);
    if (btn_handle)
    {
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, push_btn_cb);
        iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, factory_reset_trigger);
    }

    button_config_t dimmer_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = LIGHT_DIMMER_BUTTON_GPIO,
            .active_level = LIGHT_DIMMER_BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t dimmer_btn_handle = iot_button_create(&dimmer_btn_cfg);
    if (dimmer_btn_handle)
    {
        iot_button_register_cb(dimmer_btn_handle, BUTTON_SINGLE_CLICK, dimmer_btn_cb);
    }

    /* WCMCU-2812B-8 is treated as an 8-pixel WS2812B-compatible ring. */
    led_strip_config_t strip_config = {
        .strip_gpio_num = LIGHT_STRIP_GPIO,
        .max_leds = LIGHT_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = false},
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {.with_dma = false},
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip));
    ESP_ERROR_CHECK(led_strip_clear(g_led_strip));
    ESP_ERROR_CHECK(render_led());
}

int app_driver_set_state(bool state)
{
    if (g_output_state != state)
    {
        g_output_state = state;
        ESP_LOGI(TAG, "%s", state ? "Light ON" : "Light OFF");
        return render_led();
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_output_state;
}

esp_err_t app_light_set_power(bool power)
{
    return (esp_err_t)app_driver_set_state(power);
}

esp_err_t app_light_set(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    stop_rainbow_if_running();
    g_hue = (uint16_t)(hue % 360U);
    g_saturation = (uint8_t)(saturation > 100U ? 100U : saturation);
    g_brightness = (uint16_t)(brightness > 100U ? 100U : brightness);
    return render_led();
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_brightness = brightness > 100U ? 100U : brightness;
    return render_led();
}

esp_err_t app_light_set_hue(uint16_t hue)
{
    stop_rainbow_if_running();
    g_hue = hue % 360U;
    return render_led();
}

esp_err_t app_light_set_saturation(uint16_t saturation)
{
    stop_rainbow_if_running();
    g_saturation = saturation > 100U ? 100U : (uint8_t)saturation;
    return render_led();
}

/*
 * Enable/disable the rainbow effect.
 * Add these prototypes to app_priv.h:
 *     esp_err_t app_light_set_rainbow(bool enabled);
 *     esp_err_t app_light_set_dimming(bool enabled);
 */
esp_err_t app_light_set_rainbow(bool enabled)
{
    // return ESP_ERR_NOT_SUPPORTED;

    if (enabled == g_is_rainbow)
        return ESP_OK;

    if (enabled)
    {
        /* Only one animated effect can run at a time. */
        if (g_is_dimming)
        {
            esp_err_t err = app_light_set_dimming(false);
            if (err != ESP_OK)
                return err;
        }
        stop_blink_if_running();
        stop_meteor_if_running();

        if (!g_output_state)
        {
            int ret = app_driver_set_state(true);
            if (ret != ESP_OK)
                return (esp_err_t)ret;
        }

        if (g_rainbow_timer == NULL)
        {
            g_rainbow_timer = xTimerCreate("rainbow_timer",
                                           pdMS_TO_TICKS(RAINBOW_TICK_MS), pdTRUE, NULL,
                                           rainbow_timer_cb);
            if (g_rainbow_timer == NULL)
                return ESP_ERR_NO_MEM;
        }

        g_is_rainbow = true;
        g_rainbow_hue = 0;
        esp_err_t err = render_led();
        if (err != ESP_OK)
        {
            g_is_rainbow = false;
            return err;
        }

        if (xTimerStart(g_rainbow_timer, 0) != pdPASS)
        {
            g_is_rainbow = false;
            (void)render_led();
            return ESP_FAIL;
        }

        report_bool(rainbow_param, true);
        sync_effect_param();
        ESP_LOGI(TAG, "Rainbow: ON");
        return ESP_OK;
    }

    stop_rainbow_if_running(); /* also reports Rainbow = false */
    return render_led();
}

esp_err_t app_light_set_meteor(bool enabled)
{
    if (enabled == g_is_meteor)
        return ESP_OK;

    if (enabled)
    {
        if (g_is_dimming)
        {
            esp_err_t err = app_light_set_dimming(false);
            if (err != ESP_OK)
                return err;
        }
        stop_blink_if_running();
        stop_rainbow_if_running();

        if (!g_output_state)
        {
            int ret = app_driver_set_state(true);
            if (ret != ESP_OK)
                return (esp_err_t)ret;
        }

        if (g_meteor_timer == NULL)
        {
            g_meteor_timer = xTimerCreate("meteor_timer", pdMS_TO_TICKS(METEOR_TICK_MS),
                                          pdTRUE, NULL, meteor_timer_cb);
            if (g_meteor_timer == NULL)
                return ESP_ERR_NO_MEM;
        }

        g_is_meteor = true;
        g_meteor_head = 0;
        esp_err_t err = render_led();
        if (err != ESP_OK)
        {
            g_is_meteor = false;
            return err;
        }

        if (xTimerStart(g_meteor_timer, 0) != pdPASS)
        {
            g_is_meteor = false;
            (void)render_led();
            return ESP_FAIL;
        }

        report_bool(meteor_param, true);
        sync_effect_param();
        ESP_LOGI(TAG, "Meteor: ON");
        return ESP_OK;
    }

    stop_meteor_if_running();
    return render_led();
}

esp_err_t app_light_set_blink(bool enabled)
{
    if (enabled == g_is_blinking)
        return ESP_OK;

    if (enabled)
    {
        if (g_is_dimming)
        {
            esp_err_t err = app_light_set_dimming(false);
            if (err != ESP_OK)
                return err;
        }
        stop_rainbow_if_running();
        stop_meteor_if_running();

        if (!g_output_state)
        {
            int ret = app_driver_set_state(true);
            if (ret != ESP_OK)
                return (esp_err_t)ret;
        }

        if (g_blink_timer == NULL)
        {
            g_blink_timer = xTimerCreate("blink_timer", pdMS_TO_TICKS(BLINK_TICK_MS),
                                         pdTRUE, NULL, blink_timer_cb);
            if (g_blink_timer == NULL)
                return ESP_ERR_NO_MEM;
        }

        g_is_blinking = true;
        g_blink_visible = true;
        esp_err_t err = render_led();
        if (err != ESP_OK)
        {
            g_is_blinking = false;
            return err;
        }

        if (xTimerStart(g_blink_timer, 0) != pdPASS)
        {
            g_is_blinking = false;
            (void)render_led();
            return ESP_FAIL;
        }

        report_bool(blink_param, true);
        sync_effect_param();
        ESP_LOGI(TAG, "Blink: ON");
        return ESP_OK;
    }

    stop_blink_if_running();
    return render_led();
}

esp_err_t app_light_set_dimming(bool enabled)
{
    if (enabled == g_is_dimming)
        return ESP_OK;

    if (enabled)
    {
        /* Only one animated effect can run at a time. */
        stop_rainbow_if_running();
        stop_blink_if_running();
        stop_meteor_if_running();

        if (!g_output_state)
        {
            esp_err_t err = (esp_err_t)app_driver_set_state(true);
            if (err != ESP_OK)
                return err;
        }

        if (g_dim_timer == NULL)
        {
            g_dim_timer = xTimerCreate("dim_timer", pdMS_TO_TICKS(DIM_TICK_MS),
                                       pdTRUE, NULL, dim_timer_cb);
            if (g_dim_timer == NULL)
                return ESP_ERR_NO_MEM;
        }

        /* Set the flag before starting the timer, revert on failure. */
        g_is_dimming = true;
        if (xTimerStart(g_dim_timer, 0) != pdPASS)
        {
            g_is_dimming = false;
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Dimming: ON");
    }
    else
    {
        g_is_dimming = false;
        if (g_dim_timer)
            (void)xTimerStop(g_dim_timer, 0);
        ESP_LOGI(TAG, "Dimming: OFF, value=%u", (unsigned)g_brightness);
    }

    report_bool(dimming_param, enabled);
    sync_effect_param();
    return ESP_OK;
}

esp_err_t app_light_select_effect(const char *effect)
{
    if (effect == NULL)
        return ESP_ERR_INVALID_ARG;

    if (strcmp(effect, APP_LIGHT_EFFECT_STATIC) == 0)
    {
        if (g_is_dimming)
            (void)app_light_set_dimming(false);
        if (g_is_blinking)
            (void)app_light_set_blink(false);
        if (g_is_rainbow)
            (void)app_light_set_rainbow(false);
        if (g_is_meteor)
            (void)app_light_set_meteor(false);
        sync_effect_param();
        return render_led();
    }
    if (strcmp(effect, APP_LIGHT_EFFECT_BREATHING) == 0)
        return app_light_set_dimming(true);
    if (strcmp(effect, APP_LIGHT_EFFECT_BLINK) == 0)
        return app_light_set_blink(true);
    if (strcmp(effect, APP_LIGHT_EFFECT_RAINBOW_WAVE) == 0)
        return app_light_set_rainbow(true);
    if (strcmp(effect, APP_LIGHT_EFFECT_METEOR) == 0)
        return app_light_set_meteor(true);

    ESP_LOGW(TAG, "Unsupported effect: %s", effect);
    return ESP_ERR_INVALID_ARG;
}
