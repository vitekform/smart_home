idf_component_register(SRCS "main.cpp" "modules/fastfetch.cpp"
        INCLUDE_DIRS "."
        PRIV_REQUIRES esp_wifi lwip esp_event spi_flash nvs_flash esp_timer)