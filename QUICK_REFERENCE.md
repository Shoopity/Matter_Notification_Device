# ESP-IDF Quick Reference Commands

This guide provides a quick reference for common commands used when building, flashing, and cleaning this ESP-IDF project.

## Full Clean (Deep Clean)

If you are experiencing build issues, changing targets, or need to reset your environment to a clean state, run this command inside your project directory (e.g., `light/` or `generic_switch/`):

```bash
rm -rf build sdkconfig sdkconfig.old managed_components dependencies.lock .shadow-plugin-configured
```

This will completely remove the build cache, current configuration, and downloaded components.

## Setting the Target

Whenever you perform a full clean, or open the project for the first time, you must set the target MCU. This project uses the `esp32c6`:

```bash
idf.py set-target esp32c6
```

*Note: Running `set-target` will automatically create a new default `sdkconfig` file.*

## Standard Build Cycle

**1. Configure the Project (Optional):**
To change board settings, pins, or LED types:
```bash
idf.py menuconfig
```

**2. Build the Firmware:**
```bash
idf.py build
```

**3. Flash the Device:**
Replace `COMx` (Windows) or `/dev/ttyUSB0` / `/dev/ttyACM0` (Linux/Mac) with your device's serial port:
```bash
idf.py -p /dev/ttyACM0 flash
```

**4. Monitor Logs:**
To view the serial output (useful for getting the Matter QR code or debugging):
```bash
idf.py -p /dev/ttyACM0 monitor
```
*(To exit the monitor, press `Ctrl + ]`)*

**5. Flash & Monitor (Combined):**
```bash
idf.py -p /dev/ttyACM0 flash monitor
```

## Useful ESP-Matter Commands

If you need to completely erase the flash (useful to clear Matter commissioning data before repairing):
```bash
idf.py -p /dev/ttyACM0 erase-flash
```
