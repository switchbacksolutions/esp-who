#include "pedestrian_detect.hpp"
#include "who_detect_app.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <string.h>

using namespace who::cam;
using namespace who::lcd;
using namespace who::app;

#define WITH_LCD 1
#define WITH_STREAM 1  // Enable HTTP streaming mode

// WiFi credentials - TODO: Configure these or use WiFi provisioning
#define WIFI_SSID      "INSERTHERE"
#define WIFI_PASSWORD  "INSERTHERE"

static bool wifi_connected = false;
static esp_netif_ip_info_t wifi_ip_info = {};

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI("wifi", "WiFi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI("wifi", "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        wifi_ip_info = event->ip_info;
        wifi_connected = true;
        ESP_LOGI("wifi", "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi", "WiFi initialization finished. Connecting to %s...", WIFI_SSID);
}

static bool wait_for_ip(void)
{
    int retries = 0;
    const int max_retries = 100; // 100 * 200ms = 20 seconds
    
    ESP_LOGI("wifi", "Waiting for WiFi connection and IP address...");
    while (retries < max_retries) {
        if (wifi_connected && wifi_ip_info.ip.addr != 0) {
            ESP_LOGI("wifi", "WiFi connected! IP: " IPSTR, IP2STR(&wifi_ip_info.ip));
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        retries++;
        if (retries % 10 == 0) {
            ESP_LOGI("wifi", "Still waiting for WiFi connection... (%d/%d)", retries, max_retries);
        }
    }
    ESP_LOGE("wifi", "WiFi connection timeout after %d seconds", max_retries * 200 / 1000);
    return false;
}

extern "C" void app_main(void)
{
    ESP_LOGI("app_main", "Starting app_main");
    
#if WITH_STREAM
    // Initialize WiFi for HTTP streaming
    ESP_LOGI("app_main", "Initializing WiFi for HTTP streaming");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    wifi_init_sta();
    if (!wait_for_ip()) {
        ESP_LOGE("app_main", "Failed to connect to WiFi. HTTP server will not start.");
        ESP_LOGE("app_main", "Please check WiFi credentials and network availability.");
        // Continue anyway - user can check logs
    } else {
        ESP_LOGI("app_main", "WiFi ready, HTTP server can be started");
    }
#endif
#if CONFIG_PEDESTRIAN_DETECT_MODEL_IN_SDCARD
    ESP_LOGI("app_main", "Mounting SD card");
    ESP_ERROR_CHECK(bsp_sdcard_mount());
#endif

#if CONFIG_IDF_TARGET_ESP32S3
    ESP_LOGI("app_main", "Initializing LEDs");
    ESP_ERROR_CHECK(bsp_leds_init());
    ESP_ERROR_CHECK(bsp_led_set(BSP_LED_GREEN, false));
#endif

#if CONFIG_IDF_TARGET_ESP32P4
    auto cam = new WhoP4Cam(VIDEO_PIX_FMT_RGB565, 3, V4L2_MEMORY_USERPTR, true);
    // auto cam = new WhoP4PPACam(VIDEO_PIX_FMT_RGB565, 4, V4L2_MEMORY_USERPTR, 224, 224, true);
#elif CONFIG_IDF_TARGET_ESP32S3
    ESP_LOGI("app_main", "Creating camera");
    auto cam = new WhoS3Cam(PIXFORMAT_RGB565, FRAMESIZE_240X240, 2, true);
    ESP_LOGI("app_main", "Camera created successfully");
#endif
    ESP_LOGI("app_main", "Attempting to create LCD");
    WhoLCD *lcd = nullptr;
    bool lcd_available = false;
    
    // Try to initialize LCD, but don't crash if it fails
    // The LCD initialization might fail if hardware is not present
    // We'll check by trying to create it and see if we get past the initialization
    // For now, disable LCD to avoid restart loop - user can enable if hardware is present
#if WITH_LCD && 0  // Temporarily disabled to avoid restart loop
    lcd = new WhoLCD();
    lcd_available = true;
    ESP_LOGI("app_main", "LCD created successfully");
#else
    ESP_LOGW("app_main", "LCD initialization skipped (hardware may not be present)");
#endif

    auto model = new PedestrianDetect();
    // Lower score threshold to detect multiple pedestrians
    // Default is 0.7 (70% confidence), lowering to 0.5 (50%) for better multi-detection
    model->set_score_thr(0.5);

#if WITH_LCD && 0  // Temporarily disabled
    if (lcd_available && lcd != nullptr) {
        auto detect = new WhoDetectAppLCD({{255, 0, 0}});
        detect->set_cam(cam);
        detect->set_lcd(lcd);
        detect->set_model(model);
        detect->run();
    } else {
        ESP_LOGW("app_main", "Falling back to terminal mode (no LCD)");
        auto detect = new WhoDetectAppTerm();
        detect->set_cam(cam);
        detect->set_model(model);
        detect->run();
    }
#else
#if WITH_STREAM
    ESP_LOGI("app_main", "Using HTTP streaming mode");
    auto detect = new WhoDetectAppStream(80);
    detect->set_cam(cam);
    detect->set_model(model);
    detect->run();
    // Start server only if WiFi is connected
    if (wifi_connected && wifi_ip_info.ip.addr != 0) {
        if (detect->start_server()) {
            ESP_LOGI("app_main", "HTTP stream server started successfully!");
            ESP_LOGI("app_main", "Connect to http://" IPSTR "/", IP2STR(&wifi_ip_info.ip));
            ESP_LOGI("app_main", "Or access stream directly at http://" IPSTR "/stream", IP2STR(&wifi_ip_info.ip));
        } else {
            ESP_LOGE("app_main", "Failed to start HTTP server");
        }
    } else {
        ESP_LOGE("app_main", "Cannot start HTTP server - WiFi not connected");
        ESP_LOGE("app_main", "Current WiFi status: connected=%d, IP=" IPSTR, 
                 wifi_connected, IP2STR(&wifi_ip_info.ip));
    }
#else
    ESP_LOGI("app_main", "Using terminal mode (LCD disabled)");
    auto detect = new WhoDetectAppTerm();
    detect->set_cam(cam);
    detect->set_model(model);
    // detect->set_fps(5);
    detect->run();
#endif
#endif
}
