#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_matter.h>
#include <esp_matter_client.h>

#define BUTTON_GPIO GPIO_NUM_9
#define DEBOUNCE_MS 50

static const char *TAG = "app_driver";
static uint16_t g_local_endpoint_id = 0;

static void send_toggle_command()
{
    ESP_LOGI(TAG, "Sending Toggle command to bound devices...");

    // Create the Matter client request handle
    esp_matter::client::request_handle_t req_handle;
    req_handle.type = esp_matter::client::INVOKE_CMD;
    req_handle.command_path.mClusterId = chip::app::Clusters::OnOff::Id;
    req_handle.command_path.mCommandId = chip::app::Clusters::OnOff::Commands::Toggle::Id;

    // Toggle command type (empty structure, since it takes no arguments)
    chip::app::Clusters::OnOff::Commands::Toggle::Type toggle_cmd;
    req_handle.request_data = &toggle_cmd;

    // Take the CHIP stack lock before interacting with the stack
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = esp_matter::client::cluster_update(g_local_endpoint_id, &req_handle);
    esp_matter::lock::chip_stack_unlock();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send toggle command, err: %d", err);
    } else {
        ESP_LOGI(TAG, "Toggle command sent successfully!");
    }
}

static void button_task(void *pvParameters)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Set up wakeup source for light sleep
    gpio_wakeup_enable(BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    bool last_state = true; // High when not pressed (pull-up)
    
    while (true) {
        bool current_state = gpio_get_level(BUTTON_GPIO) == 1;

        if (last_state && !current_state) {
            // Button pressed (transition from high to low)
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS)); // Debounce
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Button press detected!");
                send_toggle_command();
            }
        }
        
        last_state = current_state;
        // Poll every 50ms. During this delay, if tickless idle is enabled,
        // the CPU will automatically enter light sleep.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t app_driver_init(uint16_t local_endpoint_id)
{
    g_local_endpoint_id = local_endpoint_id;

    // Initialize Power Management for Dynamic Frequency Scaling (DFS) and Automatic Light Sleep
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 80, // ESP32-C6 default max is 160MHz, but we cap at 80MHz to save battery
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Power management initialized successfully for sleepy end device.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize power management, err: %d", err);
    }
#endif

    // Start the button polling and handler task
    BaseType_t ret = xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
