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
#include <iot_button.h>
#include <button_gpio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
#define BUTTON_GPIO_PIN GPIO_NUM_0
#else // CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2
#define BUTTON_GPIO_PIN GPIO_NUM_9
#endif

// Seeed ESP32-C6 onboard LED pin
#define ONBOARD_LED_GPIO GPIO_NUM_15

// Onboard LED output levels for this board.
#define ONBOARD_LED_ON_LEVEL  0
#define ONBOARD_LED_OFF_LEVEL 1

uint16_t button_endpoint_id = 0;
uint16_t led_endpoint_id = 0;

static const char *TAG = "app_driver";
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
    ESP_LOGI(TAG, "onboard_led_set(%s): GPIO=%d", on ? "ON" : "OFF", ONBOARD_LED_GPIO);
}

using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::cluster;

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = button_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
    if (!attribute) {
        ESP_LOGE(TAG, "Failed to get attribute for button endpoint %d", endpoint_id);
        return;
    }

    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    bool next_state = !val.val.b;

    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, cluster_id, attribute_id, next_state]() {
        esp_matter_attr_val_t update_val = esp_matter_bool(next_state);
        attribute::update(endpoint_id, cluster_id, attribute_id, &update_val);
    });
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == led_endpoint_id) {
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                onboard_led_set(val->val.b);
            }
        }
    }
    return err;
}

app_driver_handle_t app_driver_button_init(gpio_button * button)
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};

    if (button != NULL) {
        const button_gpio_config_t btn_gpio_cfg = {
            .gpio_num = button->GPIO_PIN_VALUE,
            .active_level = 0,
        };
        if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create button device");
            return NULL;
        }
    } else {
        const button_gpio_config_t btn_gpio_cfg = {
            .gpio_num = BUTTON_GPIO_PIN,
            .active_level = 0,
        };
        if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create button device");
            return NULL;
        }
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
