#include "core.c"
#include "ir_rx.c"
#include "ir_tx.c"
#include "led.c"
#include "wifi.c"
#include "ws.c"

void app_main() {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);
    led_main();
    main_wifi();
    main_ir_tx();
    ws_main();
}
