#include "core.c"

void ws_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            esp_websocket_client_send_text(data->client, "Hello", 5, portMAX_DELAY);
            break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(__FILE__, "recv: %.*s", data->data_len, (char*)data->data_ptr);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED: ESP_LOGI(__FILE__, "disconnected"); break;
    }
}

void ws_main(void) {
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = "ws://192.168.0.3:3000";
    ws_cfg.task_prio = 5;
    ws_cfg.task_stack = 4096;
    ws_cfg.buffer_size = 1024;

    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_handler, NULL);
    esp_websocket_client_start(client);
}
