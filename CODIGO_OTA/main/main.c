#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_event.h"  // Cambiado de "esp_event_loop.h" a "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_spiffs.h" // Para SPIFFS

#define WIFI_SSID "MiFibra-B640"
#define WIFI_PASS "WjZqqztt"
#define OTA_URL "http://192.168.1.12:8080/firmware"  // Cambia por la URL de tu servidor

static esp_ota_handle_t ota_handle = 0;  // Declaración de la variable `ota_handle`

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI("Eventos", "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI("Eventos", "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_DATA:
            if (esp_ota_write(ota_handle, evt->data, evt->data_len) != ESP_OK) {
                ESP_LOGE("Eventos", "OTA write failed");
                return ESP_FAIL;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI("Eventos", "HTTP_EVENT_ON_FINISH");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void ota_update() {
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000 
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    const esp_partition_t *configured = esp_ota_get_running_partition();  // Partición actual
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(configured);  // Partición de actualización

    // Verificación de las particiones
    ESP_LOGI("OTA_Update", "Running from partition: %s", configured->label);
    ESP_LOGI("OTA_Update", "Update partition: %s", update_partition ? update_partition->label : "NULL");

    // Verificar si la partición de actualización es válida
    if (update_partition == NULL) {
        ESP_LOGE("OTA_Update", "No available OTA partition for update.");
        return;
    }

    // Iniciar OTA
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        ESP_LOGE("OTA_Update", "OTA begin failed!");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI("OTA_Update", "OTA successful, writing new firmware to flash.");
        if (esp_ota_end(ota_handle) != ESP_OK) {
            ESP_LOGE("OTA_Update", "OTA end failed");
            return;
        }

        if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
            ESP_LOGE("OTA_Update", "OTA set boot partition failed");
            return;
        }

        ESP_LOGI("OTA_Update", "Rebooting...");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    } else {
        ESP_LOGE("OTA_Update", "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("Wifi_Event", "Disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("Wifi_Event", "Connected to Wi-Fi, starting OTA update...");
        ota_update();
    }
}

void wifi_init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "wifi_init_sta finished.");
}

// Función para inicializar SPIFFS
void init_spiffs() {
    ESP_LOGI("SPIFSS", "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // Inicializa y monta SPIFFS
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFSS", "Failed to mount or format filesystem");
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFSS", "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI("SPIFSS", "Partition size: total: %d, used: %d", total, used);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    // Inicializa SPIFFS y escribe el archivo wifi.csv
    init_spiffs();

    ESP_LOGI("main", "Connecting to Wi-Fi...");
    wifi_init();
}
