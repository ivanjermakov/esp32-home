idf_component_register(
    SRCS "main.c"
    REQUIRES spi_flash esp_driver_gpio nvs_flash esp_wifi esp_netif esp_event esp_timer esp_driver_uart
    INCLUDE_DIRS "."
)

