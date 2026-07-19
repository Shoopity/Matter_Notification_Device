/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <common_macros.h>
#include <enable_esp_insights.h>
#include <app_priv.h>
#include <app_reset.h>
#include <app/util/attribute-storage.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

// Stuff so we can override the default discriminator and always print the QR URL
#include <platform/CHIPDeviceLayer.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/ESP32/ESP32Config.h>
#include <setup_payload/OnboardingCodesUtil.h>

static const char *TAG = "app_main";


using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

#if CONFIG_ENABLE_ESP_INSIGHTS_TRACE
extern const char insights_auth_key_start[] asm("_binary_insights_auth_key_txt_start");
extern const char insights_auth_key_end[] asm("_binary_insights_auth_key_txt_end");
#endif

namespace {
// Please refer to https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces
constexpr const uint8_t kNamespaceSwitches = 0x43;
// Switches Namespace: 0x43, tag 0 (On)
constexpr const uint8_t kTagSwitchOn = 0;
// Switches Namespace: 0x43, tag 1 (Off)
constexpr const uint8_t kTagSwitchOff = 1;

constexpr const uint8_t kNamespacePosition = 8;
// Common Position Namespace: 8, tag: 0 (Left)
constexpr const uint8_t kTagPositionLeft = 0;
// Common Position Namespace: 8, tag: 1 (Right)
constexpr const uint8_t kTagPositionRight = 1;

const Descriptor::Structs::SemanticTagStruct::Type gEp1TagList[] = {
    {.namespaceID = kNamespaceSwitches, .tag = kTagSwitchOn},
    {.namespaceID = kNamespacePosition, .tag = kTagPositionRight}
};
const Descriptor::Structs::SemanticTagStruct::Type gEp2TagList[] = {
    {.namespaceID = kNamespaceSwitches, .tag = kTagSwitchOff},
    {.namespaceID = kNamespacePosition, .tag = kTagPositionLeft}
};

}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    /* Initialize button drivers */
    app_driver_button_init(NULL);         // Onboard BOOT Button (GPIO 9)

    /* Create Generic Switch endpoint 1 to represent the Button state */
    generic_switch::config_t button_config;
    button_config.switch_cluster.feature_flags = cluster::switch_cluster::feature::momentary_switch::get_id();
    endpoint_t *ep1 = generic_switch::create(node, &button_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(ep1 != nullptr, ESP_LOGE(TAG, "Failed to create button endpoint"));
    button_endpoint_id = endpoint::get_id(ep1);

    /* Add additional features to the Switch cluster on ep1 */
    cluster_t *switch_cluster = cluster::get(ep1, Switch::Id);
    cluster::switch_cluster::feature::action_switch::add(switch_cluster);
    cluster::switch_cluster::feature::momentary_switch_multi_press::config_t msm;
    msm.multi_press_max = 5;
    cluster::switch_cluster::feature::momentary_switch_multi_press::add(switch_cluster, &msm);

    /* Create On/Off Light endpoint 2 to represent the Onboard LED feedback */
    on_off_light::config_t led_config;
    led_config.on_off.on_off = false;
    endpoint_t *ep2 = on_off_light::create(node, &led_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(ep2 != nullptr, ESP_LOGE(TAG, "Failed to create LED endpoint"));
    led_endpoint_id = endpoint::get_id(ep2);

    ESP_LOGI(TAG, "Button Endpoint created with ID %d", button_endpoint_id);
    ESP_LOGI(TAG, "LED Endpoint created with ID %d", led_endpoint_id);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    /* Safely override the discriminator in NVS */
    uint16_t current_discriminator = 0;
    
    /* Use the modern provider to GET the discriminator */
    chip::DeviceLayer::GetCommissionableDataProvider()->GetSetupDiscriminator(current_discriminator);
    
    if (current_discriminator != 3842) {
        ESP_LOGI(TAG, "Overriding hardcoded discriminator in NVS...");
        
        /* Bypass the missing setter and write directly to the ESP32 NVS backend */
        chip::DeviceLayer::Internal::ESP32Config::WriteConfigValue(
            chip::DeviceLayer::Internal::ESP32Config::kConfigKey_SetupDiscriminator,
            static_cast<uint32_t>(3842)
        );
        
        ESP_LOGI(TAG, "Rebooting to apply new discriminator...");
        esp_restart();
    }

    /* Force print the QR Code and MT Code regardless of reboot count */
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

#if CONFIG_ENABLE_ESP_INSIGHTS_TRACE
    enable_insights(insights_auth_key_start);
#endif

    endpoint::set_semantic_tags(ep1, gEp1TagList, 2);
    endpoint::set_semantic_tags(ep2, gEp2TagList, 2);

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::factoryreset_register_commands();
    esp_matter::console::init();
#endif

}
