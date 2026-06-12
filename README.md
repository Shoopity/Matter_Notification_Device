# ESP32-C6 Matter over Thread Notification Device

This project contains the firmware for two custom smart home devices using ESP32-C6 microcontrollers:
1. **Button Device (`button_device`)**: A battery-powered (Sleepy End Device) Matter Switch. When pressed, it sends a command directly to the LED controller over Thread.
2. **LED Device (`led_device`)**: A mains-powered (120V to 5V) Thread Router controlling WS2812 LEDs. When triggered, it blinks the LEDs for 5 seconds as a notification, then resets.

These devices communicate locally via Thread (e.g. bridged by an eero Pro 6e border router) and support Matter Multi-Admin, allowing them to be controlled by Google Home (voice control/automations) and bound directly to each other for local, zero-latency execution.

---

## Hardware Pinout Configurations

### 1. Button Device
* **MCU**: ESP32-C6
* **Button Pin**: GPIO 9 (Connects to GND when pushed).
  * *Note: GPIO 9 is the default `BOOT` button on standard ESP32-C6 dev kits, making it easy to test without soldering!*
* **Power**: Button Battery (CR2032) or small LiPo. CPU is configured for tickless idle and enters light sleep automatically when inactive, waking up instantly on button press.

### 2. LED Device
* **MCU**: ESP32-C6
* **LED Data Pin**: GPIO 8 (Connects to the DI pin on the WS2812 strip).
* **LED Count**: Configured in code (`LED_STRIP_NUM_LEDS`) to 8 by default.
* **Power**: Mains-powered via a 120V to 5V transformer (always-on, acts as a Thread Router).

---

## Environment Setup & Prerequisites

To compile these projects, you need to install the **ESP-IDF** toolchain and the **ESP-Matter** SDK:

1. **Install ESP-IDF** (v5.1 or newer recommended for ESP32-C6):
   Follow the [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html).
2. **Install ESP-Matter SDK**:
   Clone the repository and install its dependencies:
   ```bash
   git clone --recursive https://github.com/espressif/esp-matter.git
   cd esp-matter
   ./install.sh
   ```
3. Set up environment variables in your terminal:
   ```bash
   # In ESP-IDF directory
   . ./export.sh
   # In ESP-Matter directory
   . ./export.sh
   ```

---

## Building and Flashing

Follow these steps for **each** project (`button_device` and `led_device`):

1. Navigate to the project directory:
   ```bash
   cd button_device   # Or cd led_device
   ```
2. Set the build target to ESP32-C6:
   ```bash
   idf.py set-target esp32c6
   ```
3. Build the project:
   ```bash
   idf.py build
   ```
4. Flash and monitor the logs (replace `COMx` with your actual serial port):
   ```bash
   idf.py -p COMx flash monitor
   ```
   *Note: To exit the monitor, press `Ctrl + ]`.*

---

## Step 1: Commissioning into Google Home

Since both devices support **Multi-Admin**, you can easily pair them with Google Home for voice control and automation support, while reserving the local binding for direct, direct-to-device communication.

1. Power on your device.
2. Open the **Google Home** app on your smartphone (make sure Bluetooth and IPv6 are active on your network, and your phone is connected to the eero's Wi-Fi network).
3. Tap **+ Add** -> **New Device** -> **Matter-enabled device**.
4. Scan the QR code or enter the setup code printed in the device logs during startup (search the serial monitor output for the setup passcode or QR code URL).
5. Name the devices (e.g., "Notification Light" and "Notification Button").
6. You can now use Google Assistant: *"Hey Google, turn on the Notification Light"* to trigger the blinking notification.

---

## Step 2: Commissioning into `chip-tool` (PC)

To configure the direct, local binding between the button and the light, you must pair them with the developer utility **`chip-tool`** on your PC. You do not need a Thread radio on your PC; as long as your PC is on the same local network as the eero border router, traffic will route automatically.

Instead of re-pairing using Bluetooth on your PC, generate a pairing code from Google Home:
1. Open the Google Home app, select the device (e.g., the LED device), go to Settings -> **Linked Matter apps** -> **Link another app**.
2. This generates a temporary **numeric setup code** (e.g., `12345678901`).
3. Run the following command on your PC to pair the LED device (assigning it Node ID `100` on the `chip-tool` fabric):
   ```bash
   ./chip-tool pairing code 100 <NUMERIC_SETUP_CODE>
   ```
4. Repeat this for the Button device (assigning it Node ID `200`):
   * Generate setup code in Google Home for the Button.
   * Pair on PC:
     ```bash
     ./chip-tool pairing code 200 <NUMERIC_SETUP_CODE>
     ```

---

## Step 3: Setting Up Direct Local Binding

Now, configure the devices so the Button (Client Node `200`) can control the Light (Server Node `100`) directly over the Thread network without routing through Google Home.

### 1. Configure the Access Control List (ACL) on the LED Device
By default, the LED device will only accept command packets from the administrator (`chip-tool` / node ID `11111` or Google Home). You must tell it to allow the Button (Node `200`) to send commands:

Run this command to update the ACL on the LED device (Node `100`):
```bash
./chip-tool accesscontrol write acl '[{"fabricIndex": 1, "privilege": 5, "authMode": 2, "subjects": [11111], "targets": null}, {"fabricIndex": 1, "privilege": 3, "authMode": 2, "subjects": [200], "targets": [{"cluster": 6, "endpoint": 1, "deviceType": null}]}]' 100 0
```
* **subjects: [11111]** maintains Admin permission (5) for `chip-tool`.
* **subjects: [200]** grants Operate permission (3) to the Button device on endpoint 1, cluster 6 (OnOff cluster).

### 2. Write the Binding Table to the Button Device
Now tell the Button device (Node `200`) that its local Switch endpoint (endpoint `1`) should send its toggle signals to the LED Controller (Node `100`):

Run this command to write the binding entry:
```bash
./chip-tool binding write binding '[{"node": 100, "endpoint": 1, "cluster": 6}]' 200 1
```

### 3. Test Direct Communication
* Press the physical button (GPIO 9 / BOOT button) on the Button device.
* The WS2812 LED strip on the LED controller should flash bright blue instantly!
* Unplug your internet connection or Google Home Hub; the button will still trigger the LED strip because the binding operates 100% locally over the eero's Thread network.

---

## Step 4: Text Message (SMS) Integration

To send a text message when the button is pushed:
1. Since the button is commissioned into Google Home, the Google Home app sees the button press events.
2. In the Google Home app, create a routine:
   * **Starter**: When "Notification Button" is pressed.
   * **Action**: Trigger a webhook or automation.
3. Alternatively, you can use **Home Assistant** (connected to the same eero Thread network):
   * Create an automation that triggers when the button state changes.
   * Use the `notify.sms` service (e.g. using Twilio, IFTTT, or a custom SMS gateway) to send a text message to your specified phone number.
