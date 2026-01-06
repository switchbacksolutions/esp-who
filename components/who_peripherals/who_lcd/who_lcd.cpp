#include "who_lcd.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include <string.h>
#if BSP_CONFIG_NO_GRAPHIC_LIB
namespace who {
namespace lcd {

static const char *TAG = "WhoLCD";

#if CONFIG_IDF_TARGET_ESP32S3
WhoLCD::WhoLCD()
{
    ESP_LOGI(TAG, "WhoLCD constructor called");
    init();
    ESP_LOGI(TAG, "WhoLCD constructor completed");
}
#endif

esp_lcd_panel_handle_t WhoLCD::get_lcd_panel_handle()
{
#if CONFIG_IDF_TARGET_ESP32S3
    return m_panel_handle;
#elif CONFIG_IDF_TARGET_ESP32P4
    return m_lcd_handles.panel;
#endif
}

#if CONFIG_IDF_TARGET_ESP32S3
void WhoLCD::init()
{
    ESP_LOGI(TAG, "WhoLCD::init() called");
    ESP_LOGI(TAG, "About to read BSP LCD constants");
    int h_res = BSP_LCD_H_RES;
    int v_res = BSP_LCD_V_RES;
    int bpp = BSP_LCD_BITS_PER_PIXEL;
    ESP_LOGI(TAG, "LCD resolution: %d x %d, bits per pixel: %d", h_res, v_res, bpp);
    ESP_LOGI(TAG, "Calculating max_transfer_sz");
    int calc_sz = h_res * v_res * (bpp / 8);
    ESP_LOGI(TAG, "Calculated size: %d bytes", calc_sz);
    ESP_LOGI(TAG, "Creating bsp_display_config_t structure");
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = calc_sz,
    };
    ESP_LOGI(TAG, "Config structure created, max_transfer_sz: %lu", bsp_disp_cfg.max_transfer_sz);
    ESP_LOGI(TAG, "About to call bsp_display_new - this may crash if LCD hardware is not present");
    vTaskDelay(pdMS_TO_TICKS(100));  // Give time for log to flush
    esp_err_t ret = bsp_display_new(&bsp_disp_cfg, &m_panel_handle, &m_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new failed with error 0x%x (%s)", ret, esp_err_to_name(ret));
        ESP_LOGE(TAG, "LCD hardware may not be present or configured incorrectly");
        abort();
    }
    ESP_LOGI(TAG, "bsp_display_new succeeded");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new failed with error 0x%x (%s)", ret, esp_err_to_name(ret));
        abort();
    }
    ESP_LOGI(TAG, "Display created successfully");
    ESP_LOGI(TAG, "Display created, turning on");
    esp_lcd_panel_disp_on_off(m_panel_handle, true);
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_LOGI(TAG, "Allocating LCD buffer (%d x %d x %d bytes)", 
             BSP_LCD_H_RES, BSP_LCD_V_RES, (BSP_LCD_BITS_PER_PIXEL / 8));
    size_t buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES * (BSP_LCD_BITS_PER_PIXEL / 8);
    m_lcd_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (m_lcd_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate LCD buffer of size %zu bytes", buffer_size);
        ESP_LOGE(TAG, "This requires internal RAM with DMA capability");
        abort();
    }
    ESP_LOGI(TAG, "LCD buffer allocated successfully at %p", m_lcd_buffer);
    ESP_LOGI(TAG, "LCD initialization complete");
}

void WhoLCD::draw_full_lcd(const void *data)
{
    // Currently esp-idf doesn't support DMA for PSRAM in esp32s3.
    // Display the fb must copy it from PSRAM to internal RAM.
    // Use a static internal memory chunk to avoid alloc internal memory each time dynamically.
    memcpy(m_lcd_buffer, data, BSP_LCD_H_RES * BSP_LCD_V_RES * (BSP_LCD_BITS_PER_PIXEL / 8));
    esp_lcd_panel_draw_bitmap(m_panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, m_lcd_buffer);
}
#elif CONFIG_IDF_TARGET_ESP32P4
void WhoLCD::init()
{
    bsp_display_config_t bsp_disp_cfg = {
#if CONFIG_BSP_LCD_TYPE_HDMI
#if CONFIG_BSP_LCD_HDMI_800x600_60HZ
        .hdmi_resolution = BSP_HDMI_RES_800x600,
#elif CONFIG_BSP_LCD_HDMI_1280x720_60HZ
        .hdmi_resolution = BSP_HDMI_RES_1280x720,
#elif CONFIG_BSP_LCD_HDMI_1280x800_60HZ
        .hdmi_resolution = BSP_HDMI_RES_1280x800,
#elif CONFIG_BSP_LCD_HDMI_1920x1080_30HZ
        .hdmi_resolution = BSP_HDMI_RES_1920x1080,
#endif
#else
        .hdmi_resolution = BSP_HDMI_RES_NONE,
#endif
        .dsi_bus = {
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        }};
    bsp_display_new_with_handles(&bsp_disp_cfg, &m_lcd_handles);
    ESP_ERROR_CHECK(bsp_display_brightness_init());
    ESP_ERROR_CHECK(bsp_display_backlight_on());
}

void WhoLCD::draw_full_lcd(const void *data)
{
    esp_lcd_panel_draw_bitmap(m_lcd_handles.panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, data);
}
#endif
} // namespace lcd
} // namespace who
#endif
