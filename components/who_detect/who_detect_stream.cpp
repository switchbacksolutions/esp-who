#include "who_detect_stream.hpp"
#include "who_detect_utils.hpp"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WhoDetectStream";

namespace who {
namespace detect {

WhoDetectStream *WhoDetectStream::s_instance = nullptr;

WhoDetectStream::WhoDetectStream(frame_cap::WhoFrameCap *frame_cap, 
                                 const std::string &name,
                                 uint16_t port) :
    WhoDetectBase(frame_cap, name), 
    m_server(nullptr),
    m_port(port),
    m_result_mutex(xSemaphoreCreateMutex()),
    m_stream_task_handle(nullptr),
    m_running(false)
{
    s_instance = this;
    m_latest_result.det_res.clear();
}

WhoDetectStream::~WhoDetectStream()
{
    stop_server();
    if (m_result_mutex) {
        vSemaphoreDelete(m_result_mutex);
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void WhoDetectStream::on_new_detect_result(const result_t &result)
{
    xSemaphoreTake(m_result_mutex, portMAX_DELAY);
    m_latest_result = result;
    xSemaphoreGive(m_result_mutex);
}

static const char index_html[] = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Pedestrian Detection Stream</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; background: #000; color: #fff; }
        img { max-width: 100%; height: auto; border: 2px solid #333; }
        .info { margin: 20px; }
    </style>
</head>
<body>
    <h1>ESP32 Pedestrian Detection</h1>
    <div class="info">
        <p>Live stream with bounding boxes</p>
        <p>Resolution: 240x240</p>
    </div>
    <img src="/stream" alt="Video Stream">
</body>
</html>
)";

esp_err_t WhoDetectStream::index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WhoDetectStream::stream_handler(httpd_req_t *req)
{
    if (!s_instance) {
        return ESP_FAIL;
    }
    
    char part_buf[256];
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    
    while (s_instance->m_running) {
        xSemaphoreTake(s_instance->m_result_mutex, portMAX_DELAY);
        result_t result = s_instance->m_latest_result;
        xSemaphoreGive(s_instance->m_result_mutex);

        if (result.fb == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        who::cam::cam_fb_t *fb_copy = result.fb;

        // Draw bounding boxes on frame if there are detections
        if (!result.det_res.empty()) {
            // RGB565 format (little-endian): red = 0xF800 = {0x00, 0xF8}
            std::vector<std::vector<uint8_t>> palette = {{0x00, 0xF8}}; // Red boxes in RGB565
            draw_detect_results_on_fb(fb_copy, result.det_res, palette);
        }
        
        // Encode RGB565 frame to JPEG
        uint8_t *jpg_buf = NULL;
        size_t jpg_len = 0;
        uint8_t jpeg_quality = 50;  // Balanced quality for ~10 FPS

        if (!frame2jpg(fb_copy, jpeg_quality, &jpg_buf, &jpg_len)) {
            ESP_LOGE(TAG, "JPEG encoding failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Send MJPEG frame header
        int len = snprintf(part_buf, sizeof(part_buf),
            "\r\n--123456789000000000000987654321\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: %u\r\n\r\n",
            (unsigned int)jpg_len);

        esp_err_t ret = httpd_resp_send_chunk(req, part_buf, len);
        if (ret == ESP_OK) {
            ret = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
        }

        // Always free JPEG buffer
        free(jpg_buf);

        if (ret != ESP_OK) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // ~10 FPS
    }
    
    return ESP_OK;
}

bool WhoDetectStream::start_server()
{
    if (m_server != nullptr) {
        return true; // Already started
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = m_port;
    config.max_uri_handlers = 2;
    
    if (httpd_start(&m_server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(m_server, &index_uri);
        
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(m_server, &stream_uri);
        
        m_running = true;
        ESP_LOGI(TAG, "HTTP server started on port %d", m_port);
        ESP_LOGI(TAG, "Stream available at http://<device-ip>/stream");
        ESP_LOGI(TAG, "Web interface at http://<device-ip>/");
        return true;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return false;
}

void WhoDetectStream::stop_server()
{
    m_running = false;
    if (m_server) {
        httpd_stop(m_server);
        m_server = nullptr;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

} // namespace detect
} // namespace who

