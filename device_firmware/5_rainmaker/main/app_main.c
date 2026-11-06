
/*
 * ESP32-C3 Light Example
 *
 * OTA Demo:
 *   V1.5.1 -> RED LED
 *   V1.6.0 -> BLUE LED
 *
 * Firmware version is obtained from ESP-IDF application metadata.
 *
 * IMPORTANT:
 * Set PROJECT_VER in the top-level CMakeLists.txt:
 *
 * V1:
 *   set(PROJECT_VER "1.5.1")
 *
 * V2:
 *   set(PROJECT_VER "1.6.0")
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_err.h"

#include "esp_rmaker_core.h"
#include "esp_rmaker_standard_params.h"
#include "esp_rmaker_standard_devices.h"
#include "esp_rmaker_ota.h"
#include "esp_rmaker_schedule.h"

#include "app_wifi.h"
#include "app_storage.h"
#include "app_priv.h"


/* ============================================================
 * GLOBAL VARIABLES
 * ============================================================ */

static const char *TAG = "rainmaker";

#define FIRMWARE_VERSION_V1  "1.5.1"
#define FIRMWARE_VERSION_V2  "1.6.0"

#define FIRMWARE_HUE_RED     0
#define FIRMWARE_HUE_BLUE    240
#define FIRMWARE_HUE_GREEN   120

esp_rmaker_device_t *light_device;
esp_rmaker_param_t *dimming_param;
esp_rmaker_param_t *blink_param;
esp_rmaker_param_t *meteor_param;
esp_rmaker_param_t *rainbow_param;
esp_rmaker_param_t *effect_param;

static int s_firmware_hue = FIRMWARE_HUE_RED;
static const char *effect_options[] = {
    APP_LIGHT_EFFECT_STATIC,
    APP_LIGHT_EFFECT_BREATHING,
    APP_LIGHT_EFFECT_BLINK,
    APP_LIGHT_EFFECT_RAINBOW_WAVE,
    APP_LIGHT_EFFECT_METEOR,
};


/* ============================================================
 * FIRMWARE VERSION MANAGEMENT
 * ============================================================ */

/*
 * Detect firmware version and configure LED color.
 *
 * V1.5.1 -> RED
 * V1.6.0 -> BLUE
 */

static void app_show_firmware_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "       ESP32-C3 FIRMWARE INFORMATION");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Firmware Version : %s", app_desc->version);
    ESP_LOGI(TAG, "Project Name     : %s", app_desc->project_name);
    ESP_LOGI(TAG, "ESP-IDF Version  : %s", app_desc->idf_ver);
    ESP_LOGI(TAG, "Compile Date     : %s", app_desc->date);
    ESP_LOGI(TAG, "Compile Time     : %s", app_desc->time);

    ESP_LOGI(TAG, "========================================");

}


/* ============================================================
 * RAINMAKER WRITE CALLBACK
 * ============================================================ */

/*
 * Handle commands received from ESP RainMaker.
 */

static esp_err_t write_cb(
    const esp_rmaker_device_t *device,
    const esp_rmaker_param_t *param,
    const esp_rmaker_param_val_t val,
    void *priv_data,
    esp_rmaker_write_ctx_t *ctx)
{
    if (ctx)
    {
        ESP_LOGI(TAG, "Received write request via: %s",
                 esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);

    /* Power */
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0)
    {
        ESP_LOGI(TAG, "Power = %s for %s - %s",
                 val.val.b ? "true" : "false",
                 device_name,
                 param_name);

        app_light_set_power(val.val.b);
    }

    /* Brightness */
    else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0)
    {
        ESP_LOGI(TAG, "Brightness = %d for %s - %s",
                 val.val.i,
                 device_name,
                 param_name);

        app_light_set_brightness(val.val.i);
    }

    /* Hue */
    else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0)
    {
        /*
         * Ignore the persisted Hue restored during initialization.
         * This prevents the old Hue value from overriding the
         * LED color selected by the new firmware version.
         */

        if (ctx && ctx->src == ESP_RMAKER_REQ_SRC_INIT)
        {
            ESP_LOGI(TAG,
                     "Ignoring restored Hue = %d, firmware Hue = %d",
                     val.val.i,
                     s_firmware_hue);

            esp_rmaker_param_update(
                param,
                esp_rmaker_int(s_firmware_hue)
            );

            return ESP_OK;
        }

        ESP_LOGI(TAG, "Hue = %d for %s - %s",
                 val.val.i,
                 device_name,
                 param_name);

        app_light_set_hue(val.val.i);
    }

    /* Saturation */
    else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0)
    {
        ESP_LOGI(TAG, "Saturation = %d for %s - %s",
                 val.val.i,
                 device_name,
                 param_name);

        app_light_set_saturation(val.val.i);
    }

    /* Effect dropdown */
    else if (strcmp(param_name, "Effect") == 0)
    {
        ESP_LOGI(TAG, "Effect: %s", val.val.s);

        if (app_light_select_effect(val.val.s) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set Effect: %s", val.val.s);
            return ESP_FAIL;
        }
    }

    /* Dimming */
    else if (strcmp(param_name, "Dimming") == 0)
    {
        ESP_LOGI(TAG, "Dimming: %s",
                 val.val.b ? "ON" : "OFF");

        if (app_light_set_dimming(val.val.b) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set Dimming");
            return ESP_FAIL;
        }
    }

    /* Blink */
    else if (strcmp(param_name, "Blink") == 0)
    {
        ESP_LOGI(TAG, "Blink: %s", val.val.b ? "ON" : "OFF");

        if (app_light_set_blink(val.val.b) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set Blink");
            return ESP_FAIL;
        }
    }

    /* Meteor */
    else if (strcmp(param_name, "Meteor") == 0)
    {
        ESP_LOGI(TAG, "Meteor: %s", val.val.b ? "ON" : "OFF");

        if (app_light_set_meteor(val.val.b) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set Meteor");
            return ESP_FAIL;
        }
    }

    /* Rainbow */
    else if (strcmp(param_name, "Rainbow") == 0)
    {
        ESP_LOGI(TAG, "Rainbow: %s",
                 val.val.b ? "ON" : "OFF");

        if (app_light_set_rainbow(val.val.b) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set Rainbow");
            return ESP_FAIL;
        }
    }

    else
    {
        ESP_LOGW(TAG, "Unknown parameter: %s", param_name);
        return ESP_OK;
    }

    /* Update parameter and report to RainMaker */
    esp_rmaker_param_update_and_report(param, val);

    return ESP_OK;
}


/* ============================================================
 * MAIN APPLICATION
 * ============================================================ */

void app_main(void)
{
    int i = 0;
    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-C3 LIGHT APPLICATION STARTING");
    ESP_LOGI(TAG, "========================================");


    /* --------------------------------------------------------
     * 1. NVS initialization
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "NVS Flash initialization");

    app_storage_init();


    /* --------------------------------------------------------
     * 2. Application driver initialization
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Application driver initialization");

    app_driver_init();


    /* --------------------------------------------------------
     * 3. Firmware version
     * -------------------------------------------------------- */

    app_show_firmware_version();


    /* --------------------------------------------------------
     * 4. Wi-Fi initialization
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Wi-Fi initialization");

    app_wifi_init();


    /* --------------------------------------------------------
     * 5. Initialize ESP RainMaker
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Initializing ESP RainMaker");

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };

    esp_rmaker_node_t *node = esp_rmaker_node_init(
        &rainmaker_cfg,
        "ESP RainMaker Device",
        "Lightbulb"
    );

    if (!node)
    {
        ESP_LOGE(TAG, "Could not initialize node. Aborting!");

        vTaskDelay(pdMS_TO_TICKS(5000));
        abort();
    }


    /* --------------------------------------------------------
     * 6. Create Light device
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Creating Light device");

    light_device = esp_rmaker_lightbulb_device_create(
        "Light",
        NULL,
        true
    );

    if (!light_device)
    {
        ESP_LOGE(TAG, "Could not create Light device");
        abort();
    }

    esp_rmaker_device_add_cb(light_device, write_cb, NULL);


    /* --------------------------------------------------------
     * 7. Add standard light parameters
     * -------------------------------------------------------- */

    esp_rmaker_device_add_param(
        light_device,
        esp_rmaker_brightness_param_create(
            ESP_RMAKER_DEF_BRIGHTNESS_NAME,
            100
        )
    );

    esp_rmaker_device_add_param(
        light_device,
        esp_rmaker_hue_param_create(
            ESP_RMAKER_DEF_HUE_NAME,
            s_firmware_hue
        )
    );

    esp_rmaker_device_add_param(
        light_device,
        esp_rmaker_saturation_param_create(
            ESP_RMAKER_DEF_SATURATION_NAME,
            100
        )
    );


    /* --------------------------------------------------------
     * 8. Add Effect dropdown
     * -------------------------------------------------------- */

    effect_param = esp_rmaker_param_create(
        "Effect",
        NULL,
        esp_rmaker_str(APP_LIGHT_EFFECT_STATIC),
        PROP_FLAG_READ | PROP_FLAG_WRITE
    );

    esp_rmaker_param_add_ui_type(effect_param, "esp.ui.dropdown");
    esp_rmaker_param_add_valid_str_list(effect_param, effect_options,
                                        sizeof(effect_options) / sizeof(effect_options[0]));
    esp_rmaker_device_add_param(light_device, effect_param);


    /* --------------------------------------------------------
     * 9. Add custom Dimming parameter
     * -------------------------------------------------------- */

    dimming_param = esp_rmaker_param_create(
        "Dimming",
        NULL,
        esp_rmaker_bool(false),
        PROP_FLAG_READ | PROP_FLAG_WRITE
    );

    esp_rmaker_param_add_ui_type(
        dimming_param,
        "esp.ui.toggle"
    );

    esp_rmaker_device_add_param(
        light_device,
        dimming_param
    );


    /* --------------------------------------------------------
     * 10. Add custom Blink parameter
     * -------------------------------------------------------- */

    blink_param = esp_rmaker_param_create(
        "Blink",
        NULL,
        esp_rmaker_bool(false),
        PROP_FLAG_READ | PROP_FLAG_WRITE
    );

    esp_rmaker_param_add_ui_type(blink_param, "esp.ui.toggle");
    esp_rmaker_device_add_param(light_device, blink_param);


    /* --------------------------------------------------------
     * 11. Add custom Meteor parameter
     * -------------------------------------------------------- */

    meteor_param = esp_rmaker_param_create(
        "Meteor",
        NULL,
        esp_rmaker_bool(false),
        PROP_FLAG_READ | PROP_FLAG_WRITE
    );

    esp_rmaker_param_add_ui_type(meteor_param, "esp.ui.toggle");
    esp_rmaker_device_add_param(light_device, meteor_param);


    /* --------------------------------------------------------
     * 12. Add custom Rainbow parameter
     * -------------------------------------------------------- */

    rainbow_param = esp_rmaker_param_create(
        "Rainbow",
        NULL,
        esp_rmaker_bool(false),
        PROP_FLAG_READ | PROP_FLAG_WRITE
    );

    esp_rmaker_param_add_ui_type(
        rainbow_param,
        "esp.ui.toggle"
    );

    esp_rmaker_device_add_param(
        light_device,
        rainbow_param
    );


    /* --------------------------------------------------------
     * 13. Add device to RainMaker node
     * -------------------------------------------------------- */

    esp_rmaker_node_add_device(node, light_device);


    /* --------------------------------------------------------
     * 11. Enable ESP RainMaker OTA
     *
     * OTA_USING_TOPICS:
     *   - Receive OTA URL through MQTT topics.
     *   - Download firmware through HTTPS.
     *
     * Uses the default RainMaker OTA server certificate.
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Enabling ESP RainMaker OTA");

    esp_rmaker_ota_config_t ota_config = {
        .server_cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
    };

    err = esp_rmaker_ota_enable(
        &ota_config,
        OTA_USING_TOPICS
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not enable OTA: %s",
                 esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "ESP RainMaker OTA enabled");
    }


    /* --------------------------------------------------------
     * 12. Enable timezone service
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Enabling timezone service");

    esp_rmaker_timezone_service_enable();


    /* --------------------------------------------------------
     * 13. Enable scheduling
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Enabling scheduling");

    esp_rmaker_schedule_enable();


    /* --------------------------------------------------------
     * 14. Start ESP RainMaker Agent
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Starting ESP RainMaker Agent");

    esp_rmaker_start();


    /* --------------------------------------------------------
     * 15. Start Wi-Fi
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "Starting Wi-Fi");

    err = app_wifi_start(POP_TYPE_RANDOM);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start Wi-Fi: %s",
                 esp_err_to_name(err));

        vTaskDelay(pdMS_TO_TICKS(5000));
        abort();
    }


    /* --------------------------------------------------------
     * 16. Firmware monitoring loop
     * -------------------------------------------------------- */

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "APPLICATION INITIALIZED");
    ESP_LOGI(TAG, "Waiting for OTA requests...");
    ESP_LOGI(TAG, "========================================");

    while (1)
    {
        const esp_app_desc_t *app_desc = esp_app_get_description();

        ESP_LOGI(
            TAG,
            "[%02d] Running firmware version: %s",
            i++,
            app_desc->version
        );

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
