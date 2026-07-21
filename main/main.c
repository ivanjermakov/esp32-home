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

typedef struct {
    uint16_t t1;
    int16_t t2;
    int16_t t3;
} Calibration;

Calibration calibration;

float compensate_temperature(int32_t raw_temp) {
    int32_t var1 = ((((raw_temp >> 3) - (calibration.t1 << 1))) * (calibration.t2)) >> 11;
    int32_t var2 =
        (((((raw_temp >> 4) - calibration.t1) * ((raw_temp >> 4) - calibration.t1)) >> 12) *
         (calibration.t3)) >>
        14;
    return (float)(((var1 + var2) * 5 + 128) >> 8) / 100.0;
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

    // calib
    uint8_t calib_data_raw[6];
    write_buf[0] = 0x88;
    i2c_master_transmit_receive(dev_handle, write_buf, 1, calib_data_raw, 6, -1);
    calibration.t1 = (calib_data_raw[1] << 8) | calib_data_raw[0];
    calibration.t2 = (calib_data_raw[3] << 8) | calib_data_raw[2];
    calibration.t3 = (calib_data_raw[5] << 8) | calib_data_raw[4];
    printf("calibration: T1=%d, T2=%d, T3=%d\n", calibration.t1, calibration.t2, calibration.t3);

    while (true) {
        // ctrl_meas
        write_buf[0] = 0xf4;
        write_buf[1] = (0x01 << 5) | (0x01 << 2) | 0x01;
        i2c_master_transmit(dev_handle, write_buf, 2, -1);

        vTaskDelay(20 / portTICK_PERIOD_MS);

        // read
        write_buf[0] = 0xf7;
        ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, write_buf, 1, read_buf, 8, -1));
        int32_t raw_temp = (read_buf[3] << 12) | (read_buf[4] << 4) | (read_buf[5] >> 4);
        printf("temp: %f \n", compensate_temperature(raw_temp));
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
