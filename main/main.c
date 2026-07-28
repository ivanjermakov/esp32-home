#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "stdio.h"

#define IR_TX_GPIO 4
#define IR_RX_GPIO 15

uint32_t code_on[] = {
    4427, 4373, 565, 1591, 563,  514,  564, 1589, 566, 1589, 564, 513,  564, 513,  566, 1589,
    564,  513,  564, 511,  566,  1591, 563, 514,  564, 513,  562, 1592, 563, 1591, 564, 513,
    564,  1591, 564, 1590, 563,  513,  565, 513,  564, 1590, 564, 1591, 563, 1591, 563, 1591,
    564,  1590, 563, 514,  566,  1588, 564, 1591, 563, 515,  564, 513,  564, 512,  566, 512,
    564,  513,  565, 512,  565,  512,  565, 1589, 566, 512,  566, 511,  565, 512,  566, 510,
    567,  511,  567, 1588, 567,  1587, 567, 510,  567, 1587, 568, 1587, 568, 1586, 568, 1586,
    568,  1586, 592, 5162, 4430, 4371, 590, 1564, 590, 488,  590, 1564, 567, 1587, 566, 512,
    565,  513,  564, 1588, 565,  513,  565, 512,  565, 1589, 564, 514,  564, 513,  564, 1590,
    563,  1591, 563, 514,  563,  1591, 563, 1592, 563, 514,  562, 515,  563, 1591, 562, 1593,
    561,  1594, 560, 1594, 561,  1593, 560, 518,  559, 1595, 560, 1595, 558, 518,  559, 521,
    556,  520,  557, 539,  538,  540,  537, 540,  538, 539,  537, 1618, 536, 542,  534, 543,
    535,  542,  535, 542,  535,  543,  533, 1622, 533, 1621, 532, 545,  532, 1623, 531, 1623,
    530,  1625, 529, 1626, 527,  1629, 525,
};
uint32_t code_off[] = {
    4427, 4374, 565, 1589, 565,  512,  566, 1587, 567, 1589, 565, 510,  568, 511,  565, 1589,
    564,  514,  564, 512,  567,  1588, 564, 512,  567, 510,  568, 1586, 568, 1587, 564, 514,
    565,  1588, 568, 509,  565,  1590, 566, 1588, 566, 1588, 566, 1589, 565, 511,  566, 1589,
    567,  1587, 564, 1590, 566,  511,  566, 511,  565, 513,  565, 512,  566, 1588, 564, 512,
    566,  512,  567, 1587, 567,  1588, 565, 1589, 565, 511,  569, 509,  566, 511,  567, 510,
    568,  509,  567, 510,  568,  510,  567, 509,  567, 1587, 565, 1590, 565, 1589, 565, 1589,
    565,  1589, 565, 5189, 4427, 4373, 566, 1589, 564, 512,  565, 1590, 566, 1588, 564, 512,
    567,  510,  567, 1588, 566,  511,  566, 511,  566, 1589, 565, 512,  565, 511,  566, 1589,
    567,  1587, 566, 511,  565,  1590, 566, 511,  565, 1589, 564, 1590, 565, 1589, 565, 1589,
    564,  513,  564, 1590, 564,  1591, 563, 1591, 563, 514,  563, 514,  563, 513,  564, 514,
    563,  1591, 562, 515,  562,  516,  561, 1592, 563, 1592, 562, 1591, 562, 517,  560, 516,
    561,  516,  561, 517,  560,  517,  560, 517,  560, 518,  558, 519,  558, 1596, 558, 1596,
    558,  1617, 537, 1617, 538,  1616, 537,
};

int64_t start_t = 0;
uint32_t code_buf[512] = {0};
uint32_t code_len = 0;

void IRAM_ATTR ir_rx_isr(void* arg) {
    int64_t now = esp_timer_get_time();
    uint32_t dt = now - start_t;

    if (dt < 100 * 1000) {
        code_buf[code_len++] = dt;
    } else {
        code_len = 0;
    }

    start_t = now;
}

void ir_nec_send_timing(uint32_t on, uint32_t off) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023 / 3);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    esp_rom_delay_us(on);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    esp_rom_delay_us(off);
}

void ir_tx_code(uint32_t* code, uint32_t len) {
    for (uint32_t i = 0; i < len - 1; i += 2) {
        ir_nec_send_timing(code[i], code[i + 1]);
    }
    ir_nec_send_timing(560, 0);
}

void ir_tx_task(void* arg) {
    while (true) {
        printf(" > code_on\n");
        ir_tx_code(code_on, sizeof(code_on) / sizeof(uint32_t));
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf(" > code_off\n");
        ir_tx_code(code_off, sizeof(code_off) / sizeof(uint32_t));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ir_rx_task(void* arg) {
    while (true) {
        if (code_len > 0) {
            for (uint32_t i = 0; i < code_len; i++) {
                printf("%ld ", code_buf[i]);
            }
            printf("\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main_rx() {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << IR_RX_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

    gpio_isr_handler_add(IR_RX_GPIO, ir_rx_isr, NULL);
    xTaskCreate(ir_rx_task, "ir_rx", 2048, NULL, 5, NULL);
}

void app_main_tx() {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 38000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = IR_TX_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);

    xTaskCreate(ir_tx_task, "ir_tx", 2048, NULL, 10, NULL);
}

void app_main() {
    app_main_tx();
}
