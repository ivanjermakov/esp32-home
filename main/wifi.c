#include "core.c"

static SemaphoreHandle_t semaphore_ip = NULL;

void got_ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    xSemaphoreGive(semaphore_ip);
}

void wifi_event_handler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* evt = (wifi_event_sta_disconnected_t*)data;
        ESP_LOGI(__FILE__, "disconnect, reason: %d", evt->reason);
        esp_wifi_connect();
    }
}

void wifi_connect(void) {
    ESP_LOGI(__FILE__, "%s: connecting", CONFIG_WIFI_SSID);

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t config = {
        .sta =
            {
                .ssid = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,
                .pmf_cfg = {.capable = false, .required = false},
            },
    };
    esp_wifi_set_config(WIFI_IF_STA, &config);

    semaphore_ip = xSemaphoreCreateBinary();
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL,
                                        NULL);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
                                        NULL);
    esp_wifi_start();
    esp_wifi_connect();

    xSemaphoreTake(semaphore_ip, portMAX_DELAY);
    vSemaphoreDelete(semaphore_ip);
    ESP_LOGI(__FILE__, "%s: connected", CONFIG_WIFI_SSID);
}

void main_wifi(void) {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);

    nvs_flash_init();
    esp_netif_init();
    wifi_connect();
}
