set(src_dirs        ./)

set(include_dirs    ./)

set(requires who_detect_app
             pedestrian_detect
             esp_wifi
             esp_netif
             nvs_flash
             esp_event)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
