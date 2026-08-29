Here, the compiled Diptyx firmware can be found. 

To enter flash mode on your device, make sure it is fully turned off for at least 30 seconds. Then, press and hold the center joystick whilst connecting the device with a USB type-C cable to your pc. 

From there, you can flash the firmware binary with your preferred ESP32 flasher (such as https://www.espboards.dev/tools/program/) Upload the diptyx_firmware file, select address 0x0000, and proceed to flash the device.

Note that currently, re-flashing the firmware will reset the device settings and the book metadata stored on the device itself (only if selected from the settings. Normally, book metadata is stored on the SD card)