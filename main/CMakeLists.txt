idf_component_register(
    SRCS
        "main.cpp"
        "tg28_pmic.cpp"
        "tg28_i2c.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
)
