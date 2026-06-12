#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_matter.h>
#include <esp_matter_console.h>

static const char *TAG = "app_main";

using namespace esp_matter;
using namespace esp_matter::endpoint;

// Forward declaration of the driver initializer
extern esp_err_t app_driver_init(uint16_t local_endpoint_id);

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    ESP_LOGD(TAG, "Attribute update callback: type: %d, endpoint_id: %d, cluster_id: %lu, attribute_id: %lu",
             type, endpoint_id, cluster_id, attribute_id);
    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %d, endpoint_id: %d, effect_id: %d, effect_variant: %d",
             type, endpoint_id, effect_id, effect_variant);
    return ESP_OK;
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address Changed");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning Complete");
            break;
        default:
            break;
    }
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    // Initialize NVS (necessary for storing Matter credentials)
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Create the Matter Node configuration
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (node == nullptr) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    // Configure and create the On/Off Switch endpoint (as a Client)
    on_off_switch::config_t switch_config;
    // We create the endpoint on the node. We flag it as normal (ENDPOINT_FLAG_NONE).
    endpoint_t *endpoint = on_off_switch::create(node, &switch_config, ENDPOINT_FLAG_NONE, NULL);
    if (endpoint == nullptr) {
        ESP_LOGE(TAG, "Failed to create On/Off Switch endpoint");
        return;
    }

    uint16_t endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "On/Off Switch endpoint created with ID: %d", endpoint_id);

    // Initialize the button driver with the endpoint ID so it knows where to send commands
    err = app_driver_init(endpoint_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize app driver");
        return;
    }

    // Start the Matter stack
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter stack, err: %d", err);
        return;
    }

    // Start the console for debugging and pairing information
#if CONFIG_ESP_MATTER_CONSOLE_ENABLE
    esp_matter_console_init();
#endif
}
