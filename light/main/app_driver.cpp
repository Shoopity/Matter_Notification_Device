/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
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

// Seeed ESP32-C6 onboard LED pin?
#define ONBOARD_LED_GPIO GPIO_NUM_15

// Onboard LED output levels for this board.
#define ONBOARD_LED_ON_LEVEL  1
#define ONBOARD_LED_OFF_LEVEL 0

static bool onboard_led_initialized = false;

static void onboard_led_init(void)
{
    if (onboard_led_initialized) {
        return;
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << ONBOARD_LED_GPIO;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    onboard_led_initialized = true;
    ESP_LOGI(TAG, "onboard_led_init: GPIO=%d configured as output", ONBOARD_LED_GPIO);
}

static void onboard_led_set(bool on)
{
    onboard_led_init();
    int level = on ? ONBOARD_LED_ON_LEVEL : ONBOARD_LED_OFF_LEVEL;
    gpio_set_level(ONBOARD_LED_GPIO, level);
    int read_back = gpio_get_level(ONBOARD_LED_GPIO);
    ESP_LOGI(TAG, "onboard_led_set(%s): GPIO=%d set_level=%d read_back=%d", on ? "ON" : "OFF", ONBOARD_LED_GPIO, level, read_back);
}

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
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
                
                if (is_on) {
                    ESP_LOGI(TAG, "OnOff: turning ON - starting 5-second blink notification");
                    // Set flag to allow blinking, then launch the notification task
                    blink_active = true;
                    xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 5, NULL);
                    // Also turn on the main light via the led_driver
                    err = app_driver_light_set_power(handle, val);
                } else {
                    ESP_LOGI(TAG, "OnOff: turning OFF - stopping any active blink and turning off light");
                    // Signal the task to stop blinking (if one is running)
                    blink_active = false;
                    // Immediately turn the onboard LED off
                    onboard_led_set(false);
                    // Also turn off the main light via the led_driver
                    err = app_driver_light_set_power(handle, val);
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
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}