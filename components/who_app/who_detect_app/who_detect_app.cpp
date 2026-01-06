#include "who_detect_app.hpp"
#include "who_yield2idle.hpp"

namespace who {
namespace app {
bool WhoDetectAppBase::run()
{
    who::WhoYield2Idle::run();
    bool ret = m_frame_cap->run(4096, 2, 0);
    // Increased stack size from 2560 to 8192 words (32KB) to prevent stack overflow
    // The DetectTerm task needs more stack for image processing and model inference
    return ret & m_detect->run(8192, 2, 1);
}

WhoDetectAppLCD::WhoDetectAppLCD(const std::vector<std::vector<uint8_t>> &palette)
{
    m_frame_cap = new frame_cap::WhoFrameCapLCD("FrameCapLCD");
    m_detect = new detect::WhoDetectLCD(m_frame_cap, "DetectLCD", palette);
    add_element(m_frame_cap);
    add_element(m_detect);
}

WhoDetectAppTerm::WhoDetectAppTerm()
{
    m_frame_cap = new frame_cap::WhoFrameCap("FrameCap");
    m_detect = new detect::WhoDetectTerm(m_frame_cap, "DetectTerm");
    add_element(m_frame_cap);
    add_element(m_detect);
}

WhoDetectAppStream::WhoDetectAppStream(uint16_t port)
{
    m_frame_cap = new frame_cap::WhoFrameCap("FrameCap");
    m_detect = new detect::WhoDetectStream(m_frame_cap, "DetectStream", port);
    add_element(m_frame_cap);
    add_element(m_detect);
}

bool WhoDetectAppStream::start_server()
{
    return static_cast<detect::WhoDetectStream *>(m_detect)->start_server();
}

void WhoDetectAppStream::stop_server()
{
    static_cast<detect::WhoDetectStream *>(m_detect)->stop_server();
}

} // namespace app
} // namespace who
