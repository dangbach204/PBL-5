/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_idf_version.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
// Features supported in 4.1+
#define ESP_NETIF_SUPPORTED
#endif

#ifdef ESP_NETIF_SUPPORTED
#include <esp_netif.h>
#else
#include <tcpip_adapter.h>
#endif

#include <network_provisioning/manager.h>

#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE
#include <network_provisioning/scheme_ble.h>
#else
#include <network_provisioning/scheme_softap.h>
#endif

#include <qrcode.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "app_wifi.h"

static const char *TAG = "app_wifi";

static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

#define PROV_QR_VERSION "v1"

#define PROV_TRANSPORT_SOFTAP "softap"
#define PROV_TRANSPORT_BLE "ble"

#define QRCODE_BASE_URL "https://rainmaker.espressif.com/qrcode.html"

#define CREDENTIALS_NAMESPACE "rmaker_creds"
#define RANDOM_NVS_KEY "random"

#ifdef CONFIG_APP_WIFI_SHOW_DEMO_INTRO_TEXT

#define ESP_RAINMAKER_GITHUB_EXAMPLES_PATH \
    "https://github.com/espressif/esp-rainmaker/blob/master/examples"

#define ESP_RAINMAKER_INTRO_LINK \
    "https://rainmaker.espressif.com"

#define ESP_RMAKER_PHONE_APP_LINK \
    "http://bit.ly/esp-rmaker"

char esp_rainmaker_ascii_art[] =
    "  ______  _____ _____    _____            _____ _   _ __  __          _  ________ _____\n"
    " |  ____|/ ____|  __ \\  |  __ \\     /\\   |_   _| \\ | |  \\/  |   /\\   | |/ /  ____|  __ \\\n"
    " | |__  | (___ | |__) | | |__) |   /  \\    | | |  \\| | \\  / |  /  \\  | ' /| |__  | |__) |\n"
    " |  __|  \\___ \\|  ___/  |  _  /   / /\\ \\   | | | . ` | |\\/| | / /\\ \\ |  < |  __| |  _  /\n"
    " | |____ ____) | |      | | \\ \\  / ____ \\ _| |_| |\\  | |  | |/ ____ \\| . \\| |____| | \\ \\\n"
    " |______|_____/|_|      |_|  \\_\\/_/    \\_\\_____|_| \\_|_|  |_/_/    \\_\\_|\\_\\______|_|  \\_\\\n";

static void intro_print(bool provisioned)
{
    printf("####################################################################################################\n");
    printf("%s\n", esp_rainmaker_ascii_art);

    printf("Welcome to ESP RainMaker %s demo application!\n",
           RMAKER_DEMO_PROJECT_NAME);

    if (!provisioned)
    {
        printf("Follow these steps to get started:\n");

        printf("1. Download the ESP RainMaker phone app by visiting this link from your phone's browser:\n\n");
        printf("   %s\n\n", ESP_RMAKER_PHONE_APP_LINK);

        printf("2. Sign up and follow the steps on screen to add the device to your Wi-Fi network.\n");

        printf("3. You are now ready to use the device and control it locally as well as remotely.\n");
        printf("   You can also use the Boot button on the board to control your device.\n");
    }

    printf("\nIf you want to reset Wi-Fi credentials, or reset to factory, press and hold the Boot button.\n");

    printf("\nThis application uses ESP RainMaker, which is based on ESP IDF.\n");

    printf("Check out the source code for this application here:\n   %s/%s\n",
           ESP_RAINMAKER_GITHUB_EXAMPLES_PATH,
           RMAKER_DEMO_PROJECT_NAME);

    printf("\nPlease visit %s for additional information.\n\n",
           ESP_RAINMAKER_INTRO_LINK);

    printf("####################################################################################################\n");
}

#else

static void intro_print(bool provisioned)
{
    /* Do nothing */
}

#endif /* CONFIG_APP_WIFI_SHOW_DEMO_INTRO_TEXT */

/*
 * Print provisioning QR code
 */
static void app_wifi_print_qr(const char *name,
                              const char *pop,
                              const char *transport)
{
    if (!name || !pop || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }

    char payload[150];

    snprintf(payload,
             sizeof(payload),
             "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
             PROV_QR_VERSION,
             name,
             pop,
             transport);

#ifdef CONFIG_APP_WIFI_PROV_SHOW_QR

    ESP_LOGI(TAG,
             "Scan this QR code from the ESP RainMaker phone app for Provisioning.");

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_qrcode_generate(&cfg, payload));

#endif /* CONFIG_APP_WIFI_PROV_SHOW_QR */

    ESP_LOGI(TAG,
             "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s",
             QRCODE_BASE_URL,
             payload);
}

/*
 * Event handler for Wi-Fi, IP and provisioning events
 */
static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT)
    {

        switch (event_id)
        {

        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;

        case NETWORK_PROV_WIFI_CRED_RECV:
        {
            wifi_sta_config_t *wifi_sta_cfg =
                (wifi_sta_config_t *)event_data;

            ESP_LOGI(TAG,
                     "Received Wi-Fi credentials"
                     "\n\tSSID     : %s\n\tPassword : %s",
                     (const char *)wifi_sta_cfg->ssid,
                     (const char *)wifi_sta_cfg->password);
            break;
        }

        case NETWORK_PROV_WIFI_CRED_FAIL:
        {
            network_prov_wifi_sta_fail_reason_t *reason =
                (network_prov_wifi_sta_fail_reason_t *)event_data;

            ESP_LOGE(TAG,
                     "Provisioning failed!\n\tReason : %s"
                     "\n\tPlease reset to factory and retry provisioning",
                     (*reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            break;
        }

        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;

        case NETWORK_PROV_END:
            /*
             * De-initialize manager once provisioning is finished.
             */
            network_prov_mgr_deinit();
            break;

        default:
            break;
        }
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_START)
    {

        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG,
                 "Connected with IP Address:" IPSTR,
                 IP2STR(&event->ip_info.ip));

        /*
         * Signal main application to continue execution.
         */
        xEventGroupSetBits(wifi_event_group,
                           WIFI_CONNECTED_EVENT);
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        ESP_LOGI(TAG,
                 "Disconnected. Connecting to the AP again...");

        esp_wifi_connect();
    }
}

/*
 * Start Wi-Fi station
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/*
 * Read random bytes from factory NVS partition
 */
static esp_err_t read_random_bytes_from_nvs(uint8_t **random_bytes,
                                            size_t *len)
{
    nvs_handle handle;
    esp_err_t err;

    *len = 0;

    if ((err = nvs_open_from_partition(
             CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME,
             CREDENTIALS_NAMESPACE,
             NVS_READONLY,
             &handle)) != ESP_OK)
    {

        ESP_LOGD(TAG,
                 "NVS open for %s %s %s failed with error %d",
                 CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME,
                 CREDENTIALS_NAMESPACE,
                 RANDOM_NVS_KEY,
                 err);

        return ESP_FAIL;
    }

    if ((err = nvs_get_blob(handle,
                            RANDOM_NVS_KEY,
                            NULL,
                            len)) != ESP_OK)
    {

        ESP_LOGD(TAG,
                 "Error %d. Failed to read key %s.",
                 err,
                 RANDOM_NVS_KEY);

        nvs_close(handle);
        return ESP_ERR_NOT_FOUND;
    }

    *random_bytes = calloc(*len, 1);

    if (*random_bytes)
    {

        nvs_get_blob(handle,
                     RANDOM_NVS_KEY,
                     *random_bytes,
                     len);

        nvs_close(handle);

        return ESP_OK;
    }

    nvs_close(handle);

    return ESP_ERR_NO_MEM;
}

/*
 * Generate provisioning service name
 */
static esp_err_t get_device_service_name(char *service_name,
                                         size_t max)
{
    uint8_t *nvs_random = NULL;

    const char *ssid_prefix = "PROV_";

    size_t nvs_random_size = 0;

    if ((read_random_bytes_from_nvs(
             &nvs_random,
             &nvs_random_size) != ESP_OK) ||
        nvs_random_size < 3)
    {

        uint8_t eth_mac[6];

        esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

        snprintf(service_name,
                 max,
                 "%s%02x%02x%02x",
                 ssid_prefix,
                 eth_mac[3],
                 eth_mac[4],
                 eth_mac[5]);
    }
    else
    {

        snprintf(service_name,
                 max,
                 "%s%02x%02x%02x",
                 ssid_prefix,
                 nvs_random[nvs_random_size - 3],
                 nvs_random[nvs_random_size - 2],
                 nvs_random[nvs_random_size - 1]);
    }

    if (nvs_random)
    {
        free(nvs_random);
    }

    return ESP_OK;
}

/*
 * Generate proof-of-possession
 */
static esp_err_t get_device_pop(char *pop,
                                size_t max,
                                app_wifi_pop_type_t pop_type)
{
    if (!pop || !max)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (pop_type == POP_TYPE_MAC)
    {

        uint8_t eth_mac[6];

        esp_err_t err =
            esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

        if (err == ESP_OK)
        {

            snprintf(pop,
                     max,
                     "%02x%02x%02x%02x",
                     eth_mac[2],
                     eth_mac[3],
                     eth_mac[4],
                     eth_mac[5]);

            return ESP_OK;
        }
        else
        {
            return err;
        }
    }
    else if (pop_type == POP_TYPE_RANDOM)
    {

        uint8_t *nvs_random;

        size_t nvs_random_size = 0;

        if ((read_random_bytes_from_nvs(
                 &nvs_random,
                 &nvs_random_size) != ESP_OK) ||
            nvs_random_size < 4)
        {

            return ESP_ERR_NOT_FOUND;
        }
        else
        {

            snprintf(pop,
                     max,
                     "%02x%02x%02x%02x",
                     nvs_random[0],
                     nvs_random[1],
                     nvs_random[2],
                     nvs_random[3]);

            free(nvs_random);

            return ESP_OK;
        }
    }
    else
    {

        return ESP_ERR_INVALID_ARG;
    }
}

/*
 * Initialize Wi-Fi
 */
void app_wifi_init(void)
{
#ifdef ESP_NETIF_SUPPORTED
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    /*
     * Initialize the event loop.
     */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_event_group = xEventGroupCreate();

    /*
     * Register event handler for Wi-Fi, IP and provisioning events.
     */
    ESP_ERROR_CHECK(
        esp_event_handler_register(
            NETWORK_PROV_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL));

    /*
     * Initialize Wi-Fi including netif with default config.
     */
#ifdef ESP_NETIF_SUPPORTED
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() != NULL
                        ? ESP_OK
                        : ESP_FAIL);
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

/*
 * Start Wi-Fi provisioning / Wi-Fi station
 */
esp_err_t app_wifi_start(app_wifi_pop_type_t pop_type)
{
    network_prov_mgr_config_t config = {
#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE

        .scheme = network_prov_scheme_ble,

        .scheme_event_handler =
            NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,

#else

        .scheme = network_prov_scheme_softap,

        .scheme_event_handler =
            NETWORK_PROV_EVENT_HANDLER_NONE,

#endif
    };

    /*
     * Initialize provisioning manager.
     */
    ESP_ERROR_CHECK(
        network_prov_mgr_init(config));

    bool provisioned = false;

    /*
     * Check whether device is already provisioned.
     */
    ESP_ERROR_CHECK(
        network_prov_mgr_is_wifi_provisioned(&provisioned));

    /*
     * Device is not provisioned.
     */
    if (!provisioned)
    {

        ESP_LOGI(TAG, "Starting provisioning");

#ifdef ESP_NETIF_SUPPORTED
        ESP_ERROR_CHECK(
            esp_netif_create_default_wifi_ap() != NULL
                ? ESP_OK
                : ESP_FAIL);
#endif

        /*
         * Generate device service name.
         */
        char service_name[12];

        get_device_service_name(
            service_name,
            sizeof(service_name));

        /*
         * Security mode.
         */
        network_prov_security_t security =
            NETWORK_PROV_SECURITY_1;

        /*
         * Proof-of-possession.
         */
        char pop[9];

        esp_err_t err =
            get_device_pop(
                pop,
                sizeof(pop),
                pop_type);

        if (err != ESP_OK)
        {

            ESP_LOGE(TAG,
                     "Error: %d. Failed to get PoP from NVS, "
                     "Please perform Claiming.",
                     err);

            return err;
        }

#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE

        /*
         * Custom BLE service UUID.
         */
        uint8_t custom_service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c,
            0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03,
            0x04, 0x90, 0x1a, 0x02};

        err =
            network_prov_scheme_ble_set_service_uuid(
                custom_service_uuid);

        if (err != ESP_OK)
        {

            ESP_LOGE(TAG,
                     "network_prov_scheme_ble_set_service_uuid failed %d",
                     err);

            return err;
        }

#endif /* CONFIG_APP_WIFI_PROV_TRANSPORT_BLE */

        /*
         * Service key.
         *
         * NULL means open SoftAP network.
         * It is ignored when using BLE.
         */
        const char *service_key = NULL;

        /*
         * Start provisioning service.
         */
        ESP_ERROR_CHECK(
            network_prov_mgr_start_provisioning(
                security,
                pop,
                service_name,
                service_key));

        /*
         * Print QR code.
         */
#ifdef CONFIG_APP_WIFI_PROV_TRANSPORT_BLE

        app_wifi_print_qr(
            service_name,
            pop,
            PROV_TRANSPORT_BLE);

#else

        app_wifi_print_qr(
            service_name,
            pop,
            PROV_TRANSPORT_SOFTAP);

#endif

        intro_print(provisioned);

        ESP_LOGI(TAG,
                 "Provisioning Started. Name : %s, POP : %s",
                 service_name,
                 pop);
    }
    else
    {

        ESP_LOGI(TAG,
                 "Already provisioned, starting Wi-Fi STA");

        intro_print(provisioned);

        /*
         * We don't need the manager as device is already provisioned,
         * so release its resources.
         */
        ESP_ERROR_CHECK(
            network_prov_mgr_deinit());

        /*
         * Start Wi-Fi station.
         */
        wifi_init_sta();
    }

    /*
     * Wait for Wi-Fi connection.
     */
    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_EVENT,
        false,
        true,
        portMAX_DELAY);

    return ESP_OK;
}
