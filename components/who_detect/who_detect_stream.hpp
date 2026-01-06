#pragma once
#include "who_detect_base.hpp"
#include "esp_http_server.h"
#include <queue>
#include <mutex>

namespace who {
namespace detect {

class WhoDetectStream : public WhoDetectBase {
public:
    WhoDetectStream(frame_cap::WhoFrameCap *frame_cap, 
                    const std::string &name,
                    uint16_t port = 80);
    ~WhoDetectStream();
    
    void on_new_detect_result(const result_t &result) override;
    bool start_server();
    void stop_server();

private:
    static esp_err_t stream_handler(httpd_req_t *req);
    static esp_err_t index_handler(httpd_req_t *req);
    static void stream_task(void *pvParameters);
    
    httpd_handle_t m_server;
    uint16_t m_port;
    SemaphoreHandle_t m_result_mutex;
    std::queue<result_t> m_results;
    result_t m_latest_result;
    TaskHandle_t m_stream_task_handle;
    bool m_running;
    
    static WhoDetectStream *s_instance;
};

} // namespace detect
} // namespace who

