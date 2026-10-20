#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "iot_button.h"
#include "light_driver.h"

#include <esp_rmaker_utils.h>

#include DEVELOPMENT_BOARD
#include "app_priv.h"

#include "esp_rmaker_core.h"

extern esp_rmaker_param_t *dimming_param;

#define TAG "app_driver"

#define REBOOT_DELAY 2

#define DIM_STEP 1        /* Mỗi bước tăng/giảm độ sáng (%) */
#define DIM_TICK_MS 30    /* Chu kỳ cập nhật độ sáng khi đang dimming (ms) */
#define DIM_MIN_VALUE 5   /* Độ sáng tối thiểu (%) */
#define DIM_MAX_VALUE 100 /* Độ sáng tối đa (%) */

static bool g_output_state = true;
static uint16_t g_brightness = 100; /* độ sáng hiện tại, dùng cho dimming */

static bool g_is_dimming = false;
static TimerHandle_t g_dim_timer = NULL;
static int8_t g_dim_direction = -1;

/* ---- Forward declarations ---- */
static void push_btn_cb(void *arg);
static void factory_reset_trigger(void *arg);
static void dim_timer_cb(TimerHandle_t xTimer);
static void dimmer_btn_cb(void *arg);

static void push_btn_cb(void *arg)
{
    app_driver_set_state(!g_output_state);
}

static void factory_reset_trigger(void *arg)
{
    esp_rmaker_factory_reset(0, REBOOT_DELAY);
}

/** 
 * @brief Timer chạy định kỳ khi đang dimming, tăng/giảm brightness
 *        theo kiểu bounce qua lại giữa DIM_MIN_VALUE và DIM_MAX_VALUE
 */
static void dim_timer_cb(TimerHandle_t xTimer)
{
    if (!g_output_state)
    {
        return;
    }

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
    light_driver_set_value(g_brightness);
}

/**
 * @brief Callback nút dimmer: bấm để BẬT/TẮT chế độ dimming
 */
static void dimmer_btn_cb(void *arg)
{
    bool new_state = !g_is_dimming;

    app_light_set_dimming(new_state);

    if (dimming_param != NULL)
    {
        esp_rmaker_param_update_and_report(
            dimming_param,
            esp_rmaker_bool(new_state));
    }
}

void app_driver_init()
{
    /* Nút 1: bật/tắt đèn + factory reset (long press) */
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

    /* Nút 2: bấm để bật/tắt chế độ dimming */
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

    /* Light driver initialization */
    light_driver_config_t driver_config = {
        .gpio_red = LIGHT_GPIO_RED,
        .gpio_green = LIGHT_GPIO_GREEN,
        .gpio_blue = LIGHT_GPIO_BLUE,
        .gpio_cold = LIGHT_GPIO_COLD,
        .gpio_warm = LIGHT_GPIO_WARM,
        .fade_period_ms = LIGHT_FADE_PERIOD_MS,
        .blink_period_ms = LIGHT_BLINK_PERIOD_MS,
        .freq_hz = LIGHT_FREQ_HZ,
        .clk_cfg = LEDC_USE_APB_CLK,
        .duty_resolution = LEDC_TIMER_11_BIT,
    };
    ESP_ERROR_CHECK(light_driver_init(&driver_config));
    app_light_set_power(true);
}

int IRAM_ATTR app_driver_set_state(bool state)
{
    if (g_output_state != state)
    {
        g_output_state = state;
        ESP_LOGI(TAG, "%s", state ? "Light ON" : "Light OFF");
        light_driver_set_switch(state);
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_output_state;
}

esp_err_t app_light_set_power(bool power)
{
    app_driver_set_state(power);
    return ESP_OK;
}

esp_err_t app_light_set(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    return light_driver_set_hsv(hue, saturation, brightness);
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_brightness = brightness;
    return light_driver_set_value(brightness);
}

esp_err_t app_light_set_hue(uint16_t hue)
{
    return light_driver_set_hue(hue);
}

esp_err_t app_light_set_saturation(uint16_t saturation)
{
    return light_driver_set_saturation(saturation);
}

esp_err_t app_light_set_dimming(bool enabled)
{
    g_is_dimming = enabled;

    if (g_is_dimming)
    {
        if (!g_output_state)
        {
            app_driver_set_state(true);
        }

        if (g_dim_timer == NULL)
        {
            g_dim_timer = xTimerCreate(
                "dim_timer",
                pdMS_TO_TICKS(DIM_TICK_MS),
                pdTRUE,
                NULL,
                dim_timer_cb);
        }

        if (g_dim_timer)
        {
            xTimerStart(g_dim_timer, 0);
        }

        ESP_LOGI(TAG, "Dimming: ON");
    }
    else
    {
        if (g_dim_timer)
        {
            xTimerStop(g_dim_timer, 0);
        }

        ESP_LOGI(TAG,
                 "Dimming: OFF, value=%d",
                 g_brightness);
    }

    return ESP_OK;
}