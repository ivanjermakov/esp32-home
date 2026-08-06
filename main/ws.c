#include "core.c"
#include "ir_tx.c"

void ws_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED: {
            ESP_LOGI(__FILE__, "ws connected");
            break;
        }
        case WEBSOCKET_EVENT_DATA: {
            if (data->data_len == 0) break;
            ESP_LOGI(__FILE__, "recv %d bytes: %.*s", data->data_len, data->data_len,
                     (char*)data->data_ptr);
            ESP_LOG_BUFFER_HEX(__FILE__, data->data_ptr, data->data_len);
            if (data->data_len == 1) {
                uint8_t cmd = data->data_ptr[0];
                xQueueSend(ir_tx_queue, &cmd, 0);
            }
            break;
        }
        case WEBSOCKET_EVENT_DISCONNECTED: {
            ESP_LOGI(__FILE__, "disconnected");
            break;
        }
        case WEBSOCKET_EVENT_CLOSED: {
            ESP_LOGI(__FILE__, "closed");
            break;
        }
    }
}

void ws_main(void) {
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = "ws://home.lab.lan/ac";
    ws_cfg.enable_close_reconnect = true;
    ws_cfg.reconnect_timeout_ms = 10000;
    ws_cfg.network_timeout_ms = 10000;
    ws_cfg.task_prio = 5;
    ws_cfg.task_stack = 4096;
    ws_cfg.buffer_size = 1024;

    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_handler, NULL);
    esp_websocket_client_start(client);
}
