#include "driver/gpio.h"
#include "driver/i2c_master.h"
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

void blink_led() {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    while (true) {
        // printf("LED %s\n", blink_state ? "on" : "off");
        // gpio_set_level(LED_GPIO, blink_state);

        vTaskDelay((blink_state ? 20 : 1000) / portTICK_PERIOD_MS);
        blink_state = !blink_state;
    }
}

void app_main() {
    nvs_flash_init();
    esp_netif_init();

    // wifi_connect();

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_config, &bus_handle);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0b01110110,
        .scl_speed_hz = 100000,
    };
    i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);

    uint8_t write_buf[2];
    uint8_t read_buf[8];

    // soft reset
    write_buf[0] = 0xe0;
    write_buf[1] = 0xb6;
    i2c_master_transmit(dev_handle, write_buf, 2, -1);

    uint8_t chip_id;
    write_buf[0] = 0xd0;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, write_buf, 1, &chip_id, 1, -1));
    printf("chip id: 0x%02X (should be 0x60)\n", chip_id);

    // config
    write_buf[0] = 0xf5;
    write_buf[1] = 0x00;
    i2c_master_transmit(dev_handle, write_buf, 2, -1);

    while (true) {
        // ctrl_meas
        write_buf[0] = 0xf4;
        write_buf[1] = (0x01 << 5) | (0x01 << 2) | 0x01;
        i2c_master_transmit(dev_handle, write_buf, 2, -1);

        vTaskDelay(20 / portTICK_PERIOD_MS);

        // read
        write_buf[0] = 0xf7;
        ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, write_buf, 1, read_buf, 8, -1));
        printf("raw buf: %x %x %x %x %x %x %x %x\n", read_buf[0], read_buf[1], read_buf[2],
               read_buf[3], read_buf[4], read_buf[5], read_buf[6], read_buf[7]);
        int32_t raw_temp = (read_buf[3] << 12) | (read_buf[4] << 4) | (read_buf[5] >> 4);
        printf("raw temp: %" PRId32 "\n", raw_temp);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
