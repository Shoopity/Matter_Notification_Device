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

/* ---------- Momentary switch state ---------- */
static int  s_press_count      = 1;
static bool s_multipress       = false;
static bool s_long_press_active = false;
static const uint8_t kIdlePosition = 0;
static const uint8_t kPressPosition = 1;

static void driver_set_switch_position(uint16_t endpoint_id, uint8_t position)
{
    esp_matter_attr_val_t val = esp_matter_uint8(position);
    esp_err_t err = attribute::update(endpoint_id, Switch::Id,
                                      Switch::Attributes::CurrentPosition::Id, &val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Switch CurrentPosition update failed: %s", esp_err_to_name(err));
    }
}

/* Called on first PRESS_DOWN of any press sequence */
static void app_driver_button_initial_pressed(void *arg, void *data)
{
    if (!s_multipress) {
        ESP_LOGI(TAG, "Initial button press");
        uint16_t ep = button_endpoint_id;
        chip::DeviceLayer::SystemLayer().ScheduleLambda([ep]() {
            driver_set_switch_position(ep, kPressPosition);
            switch_cluster::event::send_initial_press(ep, kPressPosition);
        });
        s_multipress = true;
    }
}

/* Called on PRESS_UP — reset position to idle */
static void app_driver_button_release(void *arg, void *data)
{
    uint16_t ep = button_endpoint_id;
    chip::DeviceLayer::SystemLayer().ScheduleLambda([ep]() {
        driver_set_switch_position(ep, kIdlePosition);
    });
}

/* Called each time a subsequent press is detected within the multi-press window */
static void app_driver_button_multipress_ongoing(void *arg, void *data)
{
    ESP_LOGI(TAG, "Multi-press ongoing");
    uint16_t ep = button_endpoint_id;
    s_press_count++;

    /* Only emit MultiPressOngoing when MSM feature is present and AS is not */
    uint32_t cluster_id    = Switch::Id;
    uint32_t attribute_id  = Switch::Attributes::FeatureMap::Id;
    attribute_t *attr = attribute::get(ep, cluster_id, attribute_id);
    esp_matter_attr_val_t val;
    attribute::get_val(attr, &val);
    uint32_t feature_map = val.val.u32;
    uint32_t msm_flag = switch_cluster::feature::momentary_switch_multi_press::get_id();
    uint32_t as_flag  = switch_cluster::feature::action_switch::get_id();

    if ((feature_map & msm_flag) && !(feature_map & as_flag)) {
        int count = s_press_count;
        chip::DeviceLayer::SystemLayer().ScheduleLambda([ep, count]() {
            driver_set_switch_position(ep, kPressPosition);
            switch_cluster::event::send_multi_press_ongoing(ep, kPressPosition, count);
        });
    }
}

/* Called when the multi-press window closes */
static void app_driver_button_multipress_complete(void *arg, void *data)
{
    ESP_LOGI(TAG, "Multi-press complete (%d presses)", s_press_count);
    uint16_t ep = button_endpoint_id;

    /* Clamp to MultiPressMax */
    attribute_t *attr = attribute::get(ep, Switch::Id, Switch::Attributes::MultiPressMax::Id);
    esp_matter_attr_val_t val;
    attribute::get_val(attr, &val);
    uint8_t max_presses = val.val.u8;
    int total = (s_press_count > max_presses) ? 0 : s_press_count;

    chip::DeviceLayer::SystemLayer().ScheduleLambda([ep, total]() {
        driver_set_switch_position(ep, kIdlePosition);
        switch_cluster::event::send_multi_press_complete(ep, kPressPosition, total);
    });

    s_press_count = 1;
    s_multipress  = false;
}

/* Called once the long-press threshold is crossed while still holding */
static void app_driver_button_long_press_start(void *arg, void *data)
{
    ESP_LOGI(TAG, "Long press start");
    s_long_press_active = true;
    uint16_t ep = button_endpoint_id;
    chip::DeviceLayer::SystemLayer().ScheduleLambda([ep]() {
        driver_set_switch_position(ep, kPressPosition);
        switch_cluster::event::send_long_press(ep, kPressPosition);
    });
}

/* Called when the button is released after a long press.
   BUTTON_PRESS_UP does NOT reliably fire for long presses in iot_button,
   so this is the only place we can cleanly reset the state machine. */
static void app_driver_button_long_press_up(void *arg, void *data)
{
    ESP_LOGI(TAG, "Long press released");
    uint16_t ep = button_endpoint_id;
    chip::DeviceLayer::SystemLayer().ScheduleLambda([ep]() {
        driver_set_switch_position(ep, kIdlePosition);
        switch_cluster::event::send_long_release(ep, kPressPosition);
    });
    /* Reset state so the next press starts a fresh sequence */
    s_long_press_active = false;
    s_multipress        = false;
    s_press_count       = 1;
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

app_driver_handle_t app_driver_button_init(gpio_button *button)
{
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};

    /* Only the default GPIO 9 path is used — both the onboard BOOT button and
       the external tactile switch are wired in parallel to GPIO 9. */
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num    = BUTTON_GPIO_PIN,
        .active_level = 0,
    };
    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN,        NULL, app_driver_button_initial_pressed,    NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_UP,          NULL, app_driver_button_release,            NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_REPEAT,      NULL, app_driver_button_multipress_ongoing, NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_REPEAT_DONE, NULL, app_driver_button_multipress_complete,NULL);
    iot_button_register_cb(handle, BUTTON_LONG_PRESS_START,  NULL, app_driver_button_long_press_start,   NULL);
    iot_button_register_cb(handle, BUTTON_LONG_PRESS_UP,     NULL, app_driver_button_long_press_up,      NULL);

    return (app_driver_handle_t)handle;
}

