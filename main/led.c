#pragma once

#include "core.c"

SemaphoreHandle_t led_sem;

void led_task(void* arg) {
    while (true) {
        xSemaphoreTake(led_sem, portMAX_DELAY);
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(LED_GPIO, 0);
    }
}

void led_init() {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);

    led_sem = xSemaphoreCreateBinary();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

void led_main() {
    led_init();
    xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);
}
