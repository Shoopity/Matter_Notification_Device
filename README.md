# ESP32-C6 Matter over Wi-Fi Notification Device

This project contains the firmware for two custom smart home devices using ESP32-C6 microcontrollers:
1. **Button Device (`button`)**: A battery-powered (Sleepy End Device) Matter generic_switch. When pressed, it sends a command directly to the LED controller over Wi-Fi.
2. **LED Device (`light`)**: A mains-powered (120V to 5V) Matter light, configured to control WS2812 LEDs. When triggered, it blinks the LEDs for 5 seconds as a notification, then resets.

These devices communicate locally via Wi-Fi (I might add Thread later) and support Matter Multi-Admin, allowing them to be controlled by Google Home (voice control/automations) and bound directly to each other for local, zero-latency execution.

**NOTE:** I did all my work on a SEEED ESP32 C6 device.  I've tried to keep things device agnostic, but I'm horrible at doing that.  I hope you can follow these commands with whatever device you decide to use.  Pay attention to examples like telling you to use /dev/ttyACM0, as that will change depending when you bind the usb with usbipd.  Or other things like, my device has single LED on it, so the firmware uses PWM to make it light up; if your board has a WS2811/12 on it, then you'll need to change the code so it uses those drivers.  I hope this contains enough info to get in the ballpark.

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
* **Power**: Mains-powered via a 120V to 5V transformer (always-on, acts as a Thread Router... maybe?).

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

## Building and Flashing (ESP-IDF)

You can compile and upload this project either using the **ESP-IDF VSCode Extension** or the standard **ESP-IDF CLI**.

### Option A: Using the ESP-IDF VSCode Extension
1. Install the [ESP-IDF VSCode Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) in VS Code.
2. Open the project root folder `Matter_Notification_Device` in VS Code.
3. Set the active folder/project to either `button` or `light`.
4. Configure the ESP-IDF extension settings, target chipset (`esp32c6`), and SDK path/configuration if prompted.
5. Click the **Build** icon (cylinder) on the VS Code status bar to compile.
6. Connect your ESP32-C6 board via USB, choose the correct serial port, and click the **Flash** icon (lightning bolt) to upload the firmware.
7. Click the **Monitor** icon (computer screen) to open the serial monitor and view logs.

### Option B: Using the ESP-IDF Command Line Interface (CLI)
Follow these steps for **each** project (`button` and `light`):
1. Navigate to the project directory:
   ```bash
   cd button   # Or cd light
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
   *To exit the monitor, press `Ctrl + ]`.*

---

## Step 1: Commissioning into Google Home

Since both devices support **Multi-Admin**, you can easily pair them with Google Home for voice control and automation support, while reserving the local binding for direct, device-to-device communication.

1. Power on your device.
2. Open the **Google Home** app on your smartphone (ensure Bluetooth and IPv6 are active on your network, and your phone is connected to the eero's Wi-Fi network).
3. Tap **+ Add** -> **Scan QR code**.
4. Scan the QR code URL generated in the device logs during startup (printed in the console logs/serial monitor output when the device boots).
   * *If the QR code does not scan, you can choose "Set up without QR code" and enter the 11-digit numeric passcode printed in the serial monitor logs.*
5. Name the devices (e.g., "Notification Light" and "Notification Button").
6. You can now use Google Assistant: *"Hey Google, turn on the Notification Light"* to trigger the blinking notification.

---

## Step 2: Commissioning into `chip-tool` (PC)

To configure the direct, local binding between the button and the light, you must pair them with the developer utility **`chip-tool`** on your PC. You do not need a Thread radio on your PC; as long as your PC is on the same local network as the border router, traffic will route automatically.

Instead of re-pairing using Bluetooth on your PC, generate a pairing code from Google Home:
1. Open the Google Home app, select the device (e.g., the LED device), go to Settings -> **Linked Matter apps** -> **Link another app**.
2. This generates a temporary **numeric setup code** (e.g., `12345678901`) (you might need to scroll to the right to see this option.
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
