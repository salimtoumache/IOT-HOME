idf_component_register(SRCS "firebase_rtdb.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_http_client esp-tls mbedtls json esp_timer)
