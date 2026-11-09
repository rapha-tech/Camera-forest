set(srcs "ina260_read_main.c")

idf_component_register(SRCS ${srcs}
                       PRIV_REQUIRES esp_driver_i2c ina260
                       INCLUDE_DIRS ".")
