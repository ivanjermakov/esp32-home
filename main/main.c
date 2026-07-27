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

#define IR_GPIO 15

QueueHandle_t ir_queue;
uint32_t last_t = 0;
uint32_t code = 0;
uint8_t bits = 0;
bool active = false;

typedef enum {
    on = 0x5926cfb0,
    off = 0x5926bdc2,
} Code;

static void IRAM_ATTR ir_intr_handler(void* arg) {
    uint32_t now = esp_timer_get_time();
    if (last_t == 0) {
        last_t = now;
        return;
    }

    uint32_t dt = now - last_t;
    last_t = now;

    if (dt < 500) return;

    if (dt > 3000 && dt < 6000) {
        code = 0;
        bits = 0;
        active = true;
    } else if (active && bits < 32) {
        if (dt < 1500)
            code <<= 1;
        else if (dt < 3000)
            code = (code << 1) | 1;
        bits++;
        if (bits == 32) {
            active = false;
            xQueueSendFromISR(ir_queue, &code, NULL);
        }
    } else {
        active = false;
        bits = 0;
    }
}

void app_main() {

    ir_queue = xQueueCreate(256, sizeof(uint32_t));
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << IR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(IR_GPIO, ir_intr_handler, NULL);

    while (true) {
        uint32_t code = 0;
        if (xQueueReceive(ir_queue, &code, pdMS_TO_TICKS(100))) {
            printf("0x%08" PRIx32 "\n", code);
        }
    }
}
