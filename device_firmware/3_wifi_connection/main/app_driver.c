#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "iot_button.h"
#include "light_driver.h"
#include "app_storage.h"

#include DEVELOPMENT_BOARD
#include "app_priv.h"

#define TAG "app_driver"

#define LIGHT_STATUS_STORE_KEY "light_status"

#define DIM_STEP 5        /* Mỗi bước tăng/giảm độ sáng (%) */
#define DIM_TICK_MS 500   /* Chu kỳ cập nhật độ sáng khi đang dimming (ms) */
#define DIM_MIN_VALUE 5   /* Độ sáng tối thiểu (%) */
#define DIM_MAX_VALUE 100 /* Độ sáng tối đa (%) */

/* Trạng thái đèn cần lưu vào NVS */
typedef struct
{
    bool power;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
} light_status_t;

static light_status_t g_light_status = {
    .power = true,
    .hue = 0,
    .saturation = 0,
    .value = 100,
};

static bool g_output_state = true;
static bool g_is_dimming = false; /* true: đang ở chế độ tự động tăng/giảm sáng */
static TimerHandle_t g_dim_timer = NULL;
static int8_t g_dim_direction = -1; /* Hướng thay đổi độ sáng hiện tại: -1 giảm, +1 tăng */

/* ---- Forward declarations (bắt buộc do dùng chuẩn C23) ---- */
static void light_status_save(void);
static void push_btn_cb(void *arg);
static void dim_timer_cb(TimerHandle_t xTimer);
static void dimmer_btn_cb(void *arg);

/**
 * @brief Lưu trạng thái đèn hiện tại xuống NVS
 */
static void light_status_save(void)
{
    if (app_storage_set(
            LIGHT_STATUS_STORE_KEY,
            &g_light_status,
            sizeof(g_light_status)) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to save light status");
    }
}

/**
 * @brief Callback nút 1 (LIGHT_BUTTON_GPIO): bật/tắt đèn
 */
static void push_btn_cb(void *arg)
{
    app_driver_set_state(!g_output_state);
}

/**
 * @brief Timer chạy định kỳ trong lúc đang dimming, tăng/giảm Value (HSV)
 *        theo kiểu bounce qua lại giữa DIM_MIN_VALUE và DIM_MAX_VALUE
 */
static void dim_timer_cb(TimerHandle_t xTimer)
{
    // Đèn đang OFF thì không thực hiện dimming
    if (!g_output_state)
    {
        return;
    }

    int16_t new_value =
        (int16_t)g_light_status.value +
        g_dim_direction * DIM_STEP;

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

    g_light_status.value = (uint8_t)new_value;

    light_driver_set_hsv(
        g_light_status.hue,
        g_light_status.saturation,
        g_light_status.value);
}

/**
 * @brief Callback nút 2 (LIGHT_DIMMER_BUTTON_GPIO): bấm để BẬT chế độ dimming,
 *        bấm lần nữa để TẮT và lưu độ sáng hiện tại vào NVS
 */
static void dimmer_btn_cb(void *arg)
{
    g_is_dimming = !g_is_dimming;

    if (g_is_dimming)
    {
        /* Bắt đầu dimming: đảm bảo đèn đang bật trước */
        if (!g_output_state)
        {
            app_driver_set_state(true);
        }

        if (g_dim_timer == NULL)
        {
            g_dim_timer = xTimerCreate("dim_timer", pdMS_TO_TICKS(DIM_TICK_MS), pdTRUE, NULL, dim_timer_cb);
        }
        if (g_dim_timer)
        {
            xTimerStart(g_dim_timer, 0);
        }
        ESP_LOGI(TAG, "Dimming: ON");
    }
    else
    {
        /* Dừng dimming, giữ nguyên độ sáng hiện tại và lưu lại */
        if (g_dim_timer)
        {
            xTimerStop(g_dim_timer, 0);
        }
        ESP_LOGI(TAG, "Dimming: OFF, value=%d", g_light_status.value);
        light_status_save();
    }
}

void app_driver_init()
{
    /* Nút 1: bật/tắt đèn */
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

    /* Load trạng thái đã lưu từ NVS, nếu chưa có thì dùng giá trị mặc định */
    if (app_storage_get(
            LIGHT_STATUS_STORE_KEY,
            &g_light_status,
            sizeof(g_light_status)) != ESP_OK)
    {
        ESP_LOGI(TAG, "No saved light status found, using defaults");
        light_status_save();
    }

    g_output_state = g_light_status.power;
    light_driver_set_switch(g_output_state);
    light_driver_set_hsv(g_light_status.hue, g_light_status.saturation, g_light_status.value);
}

int IRAM_ATTR app_driver_set_state(bool state)
{
    if (g_output_state != state)
    {
        g_output_state = state;
        g_light_status.power = state;
        light_driver_set_switch(state);
        ESP_LOGI(TAG, "%s", state ? "Light ON" : "Light OFF");
        light_status_save();
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_output_state;
}