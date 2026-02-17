idf_component_register(
    SRCS
        "com.cpp"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
)
