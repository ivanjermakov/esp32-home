idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES spi_flash esp_driver_gpio nvs_flash
    REQUIRES esp_wifi esp_netif esp_event
    INCLUDE_DIRS "."
)

