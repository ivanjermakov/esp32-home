idf_component_register(
    SRCS "main.c"
    REQUIRES esp_wifi esp_netif esp_event spi_flash esp_driver_gpio nvs_flash esp_driver_i2c
    INCLUDE_DIRS "."
)

