#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>
#include <esp_matter.h>

#define LED_STRIP_GPIO GPIO_NUM_8
#define LED_STRIP_NUM_LEDS 8 // Adjustable for your strip length

static const char *TAG = "app_driver";
static led_strip_handle_t g_led_strip = NULL;
static uint16_t g_light_endpoint_id = 0;
static TaskHandle_t g_blink_task_handle = NULL;
static bool g_is_blinking = false;

// Task to perform the blinking effect
static void led_blink_task(void *pvParameters)
{
    g_is_blinking = true;
    ESP_LOGI(TAG, "Starting notification blink task...");

    // Blink 10 times (250ms ON, 250ms OFF = 5 seconds total)
    for (int i = 0; i < 10; i++) {
        // Turn LEDs ON (Bright Cyan/Blue)
        for (int j = 0; j < LED_STRIP_NUM_LEDS; j++) {
            led_strip_set_pixel(g_led_strip, j, 0, 180, 255);
        }
        led_strip_refresh(g_led_strip);
        vTaskDelay(pdMS_TO_TICKS(250));

        // Turn LEDs OFF
        led_strip_clear(g_led_strip);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    ESP_LOGI(TAG, "Blink task complete. Resetting Matter state to OFF...");

    // Reset Matter state to OFF (false) to reflect that the notification event is over
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    uint32_t cluster_id = chip::app::Clusters::OnOff::Id;
    uint32_t attribute_id = chip::app::Clusters::OnOff::Attributes::OnOff::Id;
    esp_matter::attribute_t *attribute = esp_matter::attribute::get(g_light_endpoint_id, cluster_id, attribute_id);
    
    if (attribute != NULL) {
        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        esp_matter::attribute::get_val(attribute, &val);
        val.val.b = false; // Set OnOff state to false
        esp_matter::attribute::update(g_light_endpoint_id, cluster_id, attribute_id, &val);
    }
    esp_matter::lock::chip_stack_unlock();

    g_is_blinking = false;
    g_blink_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t app_driver_init(uint16_t light_endpoint_id)
{
    g_light_endpoint_id = light_endpoint_id;

    // LED Strip general configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_NUM_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        }
    };

    // RMT backend configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };

    // Initialize the RMT driver for WS2812
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED strip, err: %d", err);
        return err;
    }

    // Clear strip
    led_strip_clear(g_led_strip);
    ESP_LOGI(TAG, "WS2812 LED strip driver initialized on GPIO %d.", LED_STRIP_GPIO);
    return ESP_OK;
}

esp_err_t app_driver_set_state(bool on)
{
    if (on) {
        // If commanded ON and we aren't blinking, start the blink task
        if (!g_is_blinking) {
            BaseType_t ret = xTaskCreate(led_blink_task, "led_blink_task", 4096, NULL, 5, &g_blink_task_handle);
            if (ret != pdPASS) {
                ESP_LOGE(TAG, "Failed to start blink task");
                return ESP_FAIL;
            }
        }
    } else {
        // If commanded OFF and we are blinking, stop the blink task and clear LEDs
        if (g_is_blinking && g_blink_task_handle != NULL) {
            vTaskDelete(g_blink_task_handle);
            g_blink_task_handle = NULL;
            g_is_blinking = false;
        }
        led_strip_clear(g_led_strip);
    }
    return ESP_OK;
}
