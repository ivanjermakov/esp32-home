#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"
#include "sdkconfig.h"
#include "stdio.h"

#define LED_GPIO 2

bool blink_state = false;

void app_main(void) {
  printf("Hello world!\n");

  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  while (true) {
    printf("LED %s\n", blink_state ? "on" : "off");
    gpio_set_level(LED_GPIO, blink_state);

    vTaskDelay((blink_state ? 20 : 1000) / portTICK_PERIOD_MS);
    blink_state = !blink_state;
  }
  esp_restart();
}
