#include "core.c"
#include "ir_rx.c"
#include "ir_tx.c"
#include "wifi.c"
#include "ws.c"

void app_main() {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);
    main_wifi();
    ws_main();
    // main_ir_tx();
}
