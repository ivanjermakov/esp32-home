#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "stdio.h"

#define LED_GPIO 2

bool blink_state = false;
static SemaphoreHandle_t semaphore_ip = NULL;

void got_ip_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    xSemaphoreGive(semaphore_ip);
}

void wifi_connect(void) {
    printf("%s: connecting\n", CONFIG_WIFI_SSID);

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t config = {.sta = {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASSWORD}};
    esp_wifi_set_config(WIFI_IF_STA, &config);

    semaphore_ip = xSemaphoreCreateBinary();
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL,
                                        NULL);
    esp_wifi_start();
    esp_wifi_connect();

    xSemaphoreTake(semaphore_ip, portMAX_DELAY);
    vSemaphoreDelete(semaphore_ip);
    printf("%s: connected\n", CONFIG_WIFI_SSID);
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();

    wifi_connect();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    while (true) {
        printf("LED %s\n", blink_state ? "on" : "off");
        gpio_set_level(LED_GPIO, blink_state);

        vTaskDelay((blink_state ? 20 : 1000) / portTICK_PERIOD_MS);
        blink_state = !blink_state;
    }
}
