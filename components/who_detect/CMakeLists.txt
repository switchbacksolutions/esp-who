set(include_dirs ".")

set(src_dirs ".")

set(requires who_frame_cap esp_http_server espressif__esp32-camera)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
