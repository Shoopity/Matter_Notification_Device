/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <esp_matter_client.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include <app_priv.h>
#include <common_macros.h>

#include <device.h>
#include <led_driver.h>
#include <button_gpio.h>

// Adding these to blink the on-board LED
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_BOARD_LED_TYPE_WS2812
#include "led_strip.h"
#endif

using namespace chip::app::Clusters;
using namespace esp_matter;

// Configurable board LED pin
#define ONBOARD_LED_GPIO (gpio_num_t)CONFIG_BOARD_LED_PIN

// Onboard LED output levels (for standard GPIO LED)
#define ONBOARD_LED_ON_LEVEL  0
#define ONBOARD_LED_OFF_LEVEL 1

static const char *TAG = "app_driver";
static bool onboard_led_initialized = false;

#ifdef CONFIG_BOARD_LED_TYPE_WS2812
static led_strip_handle_t led_strip;
#endif

/* Client Callbacks for outgoing commands */
static void send_command_success_callback(void *context, const chip::app::ConcreteCommandPath &command_path,
                                          const chip::app::StatusIB &status, chip::TLV::TLVReader *response_data)
{
    ESP_LOGI(TAG, "Send command success");
}

static void send_command_failure_callback(void *context, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "Send command failure: err :%" CHIP_ERROR_FORMAT, error.Format());
}

void app_driver_client_invoke_command_callback(client::peer_device_t *peer_device, client::request_handle_t *req_handle,
                                               void *priv_data)
{
    if (req_handle->type == esp_matter::client::INVOKE_CMD) {
        char command_data_str[32];
        if (req_handle->command_path.mClusterId == OnOff::Id) {
            strcpy(command_data_str, "{}");
        } else {
            ESP_LOGE(TAG, "Unsupported cluster");
            return;
        }
        client::interaction::invoke::send_request(NULL, peer_device, req_handle->command_path, command_data_str,
                                                  send_command_success_callback, send_command_failure_callback,
                                                  chip::NullOptional);
    }
}

void app_driver_client_callback(client::peer_device_t *peer_device, client::request_handle_t *req_handle,
                                void *priv_data)
{
    if (req_handle->type == esp_matter::client::INVOKE_CMD) {
        app_driver_client_invoke_command_callback(peer_device, req_handle, priv_data);
    }
}

void app_driver_client_group_invoke_command_callback(uint8_t fabric_index, client::request_handle_t *req_handle,
                                                     void *priv_data)
{
    if (req_handle->type != esp_matter::client::INVOKE_CMD) {
        return;
    }
    char command_data_str[32];
    if (req_handle->command_path.mClusterId == OnOff::Id) {
        strcpy(command_data_str, "{}");
    } else {
        ESP_LOGE(TAG, "Unsupported cluster");
        return;
    }
    client::interaction::invoke::send_group_request(fabric_index, req_handle->command_path, command_data_str);
}

static void onboard_led_init(void)
{
    if (onboard_led_initialized) {
        return;
    }

#ifdef CONFIG_BOARD_LED_TYPE_WS2812
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = ONBOARD_LED_GPIO;
    strip_config.max_leds = 1;
    
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz
    rmt_config.flags.with_dma = false;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
#else
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << ONBOARD_LED_GPIO;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
#endif

    onboard_led_initialized = true;
    ESP_LOGI(TAG, "onboard_led_init: GPIO=%d configured", ONBOARD_LED_GPIO);
}

static void onboard_led_set(bool on)
{
    onboard_led_init();
#ifdef CONFIG_BOARD_LED_TYPE_WS2812
    if (on) {
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
    ESP_LOGI(TAG, "onboard_led_set(%s): WS2812 on GPIO=%d", on ? "ON" : "OFF", ONBOARD_LED_GPIO);
#else
    int level = on ? ONBOARD_LED_ON_LEVEL : ONBOARD_LED_OFF_LEVEL;
    gpio_set_level(ONBOARD_LED_GPIO, level);
    ESP_LOGI(TAG, "onboard_led_set(%s): GPIO=%d set_level=%d", on ? "ON" : "OFF", ONBOARD_LED_GPIO, level);
#endif
}

extern uint16_t light_endpoint_id;

// Global variables to store current XY color coordinates
static uint16_t current_x = 0;
static uint16_t current_y = 0;

// Flag to control whether the blink task should continue running
static volatile bool blink_active = false;

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    return led_driver_set_power(handle, val->val.b);
}

static esp_err_t app_driver_light_set_brightness(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
    return led_driver_set_brightness(handle, value);
}

static esp_err_t app_driver_light_set_hue(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
    return led_driver_set_hue(handle, value);
}

static esp_err_t app_driver_light_set_saturation(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
    return led_driver_set_saturation(handle, value);
}

static esp_err_t app_driver_light_set_temperature(led_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    uint32_t value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
    return led_driver_set_temperature(handle, value);
}

static esp_err_t app_driver_light_set_xy(led_driver_handle_t handle, uint16_t x, uint16_t y)
{
    return led_driver_set_xy(handle, x, y);
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

// Blink the on-board LED 4 times/sec, for 5 seconds
// This task is responsive—it checks blink_active before each GPIO toggle
// so it can exit immediately if the light is turned OFF
void led_blink_task(void *pvParameter) {
    // 1. Setup the pin as an output once
    onboard_led_init();
    
    // 2. Loop 20 times (250ms per loop * 20 = 5 seconds)
    for (int i = 0; i < 20; i++) {
        // Check if blinking should continue. If OFF was called, exit immediately
        if (!blink_active) {
            ESP_LOGI(TAG, "Blink task: OFF command received, exiting early");
            vTaskDelete(NULL);
            return;
        }
        
        onboard_led_set(true);
        vTaskDelay(pdMS_TO_TICKS(125));      // Wait 125ms
        
        // Check again before LED OFF to exit quickly if needed
        if (!blink_active) {
            ESP_LOGI(TAG, "Blink task: OFF command received during ON phase, exiting");
            onboard_led_set(false); // Ensure LED is OFF before exiting
            vTaskDelete(NULL);
            return;
        }
        
        onboard_led_set(false);
        vTaskDelay(pdMS_TO_TICKS(125));      // Wait 125ms
    }
    
    // 3. After 5 seconds of blinking, leave the LED ON as the light state is ON
    onboard_led_set(true);
    ESP_LOGI(TAG, "Blink task: completed 5 seconds, LED now ON (light is ON)");
    
    // 4. Tasks must delete themselves when finished in FreeRTOS!
    vTaskDelete(NULL);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        led_driver_handle_t handle = (led_driver_handle_t)driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                
                bool is_on = val->val.b; // Get the boolean (true = on, false = off)
                
                client::request_handle_t req_handle;
                req_handle.type = esp_matter::client::INVOKE_CMD;
                req_handle.command_path.mClusterId = OnOff::Id;

                if (is_on) {
                    ESP_LOGI(TAG, "OnOff: turning ON - starting 5-second blink notification");
                    // Set flag to allow blinking, then launch the notification task
                    blink_active = true;
                    xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 5, NULL);
                    // Also turn on the main light via the led_driver
                    err = app_driver_light_set_power(handle, val);

                    // Send Matter On command to bound devices
                    req_handle.command_path.mCommandId = OnOff::Commands::On::Id;
                    client::cluster_update(light_endpoint_id, &req_handle);
                } else {
                    ESP_LOGI(TAG, "OnOff: turning OFF - stopping any active blink and turning off light");
                    // Signal the task to stop blinking (if one is running)
                    blink_active = false;
                    // Immediately turn the onboard LED off
                    onboard_led_set(false);
                    // Also turn off the main light via the led_driver
                    err = app_driver_light_set_power(handle, val);

                    // Send Matter Off command to bound devices
                    req_handle.command_path.mCommandId = OnOff::Commands::Off::Id;
                    client::cluster_update(light_endpoint_id, &req_handle);
                }
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentX::Id) {
                current_x = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            } else if (attribute_id == ColorControl::Attributes::CurrentY::Id) {
                current_y = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            }
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_driver_handle_t handle = (led_driver_handle_t)priv_data;
    esp_matter_attr_val_t val;

    /* Setting brightness */
    attribute_t *attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting color */
    attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
        /* Setting hue */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        /* Setting saturation */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
        /* Setting temperature */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY) {
        /* Setting XY coordinates */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id);
        attribute::get_val(attribute, &val);
        current_x = val.val.u16;
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id);
        attribute::get_val(attribute, &val);
        current_y = val.val.u16;
        err |= app_driver_light_set_xy(handle, current_x, current_y);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
    /* Initialize led */
    led_driver_config_t config = led_driver_get_config();
    led_driver_handle_t handle = led_driver_init(&config);
    return (app_driver_handle_t)handle;
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = (gpio_num_t)CONFIG_BOARD_BUTTON_PIN,
        .active_level = 0,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}