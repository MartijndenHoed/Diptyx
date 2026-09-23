Here, the compiled Diptyx firmware can be found. 

To enter flash mode on your device, make sure it is fully turned off for at least 20 seconds. Then, press and hold the center joystick whilst connecting the device with a USB type-C cable to your pc. 

From there, you can flash the firmware binary with your preferred ESP32 flasher (such as https://www.espboards.dev/tools/program/), in a browser that supports WebSerial (Chrome, Edge, Opera)



## Patching
To patch the firmware without resetting all settings upload the diptyx_firmware **patch** file, select address **0x10000**, and proceed to flash the device.


## Fully flashing
To fully flash the device, upload the diptyx_firmware file, select address **0x0000**, and proceed to flash the device. Re-flashing the firmware will reset the device settings and the book metadata stored on the device itself (only if selected from the settings)


After flashing, remove the USB cable, wait for 20 seconds, and boot the device with the power button.