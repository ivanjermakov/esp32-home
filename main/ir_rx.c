#include "core.c"

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

void ir_rx_task(void* arg) {
    while (true) {
        if (code_len > 0) {
            ESP_LOGI(__FILE__, "code received");
            for (uint32_t i = 0; i < code_len; i++) {
                printf("%ld ", code_buf[i]);
            }
            printf("\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void main_ir_rx() {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);

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
