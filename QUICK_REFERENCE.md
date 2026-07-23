
# ESP-IDF Quick Reference Commands

This guide provides a quick reference for common commands used when building, flashing, and cleaning this ESP-IDF project.

## Startup
Any time you start up WSL, you probably need to run the following; it doesn't hurt to run it even if you don't.  This command is something you built if you followed the Fresh_environment document; it's not a standard command.
```base
load-matter
```

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

1. **Configure the Project (Optional):** (you must run this after every clean, unless the defaults are acceptable)
	To change board settings, pins, or LED types:
	```bash
	idf.py menuconfig
	```
1. **Clear the device**
	idf.py erase-flash [optional:] -p \<usb port> (useful if you have multiple boards you're flashing at once)
	```bash
	idf.py erase-flash
	```
	```bash
	idf.py erase-flash -p /dev/ttyACM0
	```
1. **Build the Firmware:** (make sure you're in the folder you want to build/flash)
	```bash
	idf.py build
	```
1. **Flash the Device:**
	idf.py flash [optional:] -p \<usb port>
	```bash
	idf.py flash
	```
	```bash
	idf.py flash -p /dev/ttyACM0
	```
1. **Monitor Logs:**
	To view the serial output (useful for getting the Matter QR code or debugging):
	```bash
	idf.py monitor -p /dev/ttyACM0
	```
	*(To exit the monitor, press `Ctrl + ]`)*
1. **Erase, Build, Flash & Monitor (Combined): (Make sure you've got the correct -p targeted**
	```bash
	idf.py erase-flash build flash monitor -p /dev/ttyACM0
	```
