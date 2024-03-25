# homebrew_zigbee_devices

First up: nRF52840-based four-input sleepy end device

Second up: nRF52840-based hall effect contact sensor sleepy end device

1. Install nRF plugins for Visual Studio Code
2. Use the plugin to install version 2.5.1 of the toolchain and sdk
3. Add custom board definition files
4. Add the zigbee_switch_v2 and zigbee_contact_v1 directories as applications.
5. For each application, create a build using the corresponding custom board.
6. Build and flash each application's build.
7. Install four-input.js as a custom handler in zigbee2mqtt.
