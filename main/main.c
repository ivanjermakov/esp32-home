#include "core.c"
#include "ir_rx.c"
#include "ir_tx.c"

void app_main() {
    esp_log_level_set(__FILE__, ESP_LOG_VERBOSE);
    app_main_tx();
}
