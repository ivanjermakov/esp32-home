#include "driver/gpio.h"
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

QueueHandle_t ir_queue;
uint32_t bit_start_t = 0;
uint64_t code = 0;
uint8_t bits = 0;

typedef enum : uint64_t {
    code_on = 0x0000b24d9f6020df,
    code_off = 0x0000b24d7b84e01f,
} Code;

void IRAM_ATTR ir_rx_isr(void* arg) {
    uint32_t now = esp_timer_get_time();
    if (bit_start_t == 0) {
        bit_start_t = now;
        return;
    }

    uint32_t dt = now - bit_start_t;
    if (dt < 500) return;
    xQueueSendFromISR(ir_queue, &dt, NULL);

    if (dt > 3000) {
        code = 0;
        bits = 0;
        bit_start_t = now;
        return;
    }

    code <<= 1;
    code |= dt < 1500 ? 0 : 1;
    bit_start_t = now;
    bits++;
    if (bits == 32 || bits == 48) {
        xQueueSendFromISR(ir_queue, &code, NULL);
    }
}

void IRAM_ATTR ir_tx_mark(uint32_t us) {
    uint32_t n = us / 21;
    for (uint32_t i = 0; i < n; i++) {
        gpio_set_level(IR_TX_GPIO, 1);
        esp_rom_delay_us(13);
        gpio_set_level(IR_TX_GPIO, 0);
        esp_rom_delay_us(13);
    }
}

void IRAM_ATTR ir_tx_space(uint32_t us) {
    gpio_set_level(IR_TX_GPIO, 0);
    esp_rom_delay_us(us);
}

void IRAM_ATTR ir_tx_nec(uint64_t code, uint8_t size) {
    ir_tx_mark(9000);
    ir_tx_space(4500);

    uint8_t offset = 64 - size;
    for (int i = size - 1; i >= 0; i--) {
        ir_tx_mark(560);
        if ((code >> (i + offset)) & 1) {
            ir_tx_space(1690);
        } else {
            ir_tx_space(560);
        }
    }
}

void ir_tx_task(void* arg) {
    while (true) {
        printf(" > 0x%016llx\n", code_on);
        ir_tx_nec(code_on, 48);
        ir_tx_nec(code_on, 48);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ir_rx_task(void* arg) {
    while (true) {
        uint64_t v;
        if (xQueueReceive(ir_queue, &v, portMAX_DELAY)) {
            printf("<  0x%016llx (%llu)\n", v, v);
        }
    }
}

void app_main() {
    ir_queue = xQueueCreate(64, sizeof(uint64_t));

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << IR_TX_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);

    cfg = (gpio_config_t){
        .pin_bit_mask = 1ULL << IR_RX_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(IR_RX_GPIO, ir_rx_isr, NULL);

    xTaskCreate(ir_tx_task, "ir_tx", 2048, NULL, 10, NULL);
    xTaskCreate(ir_rx_task, "ir_rx", 2048, NULL, 5, NULL);
}
