# esp-idf-px4

Display the VFR_HUD using the M5 stack.   
VFR_HUD contains an indicator that appears on the heads-up display.   
This project use [this](https://github.com/mavlink/c_library_v2) library.   
![screen](https://user-images.githubusercontent.com/6020549/95003590-b118ac80-061b-11eb-914a-960be8d0ea9e.JPG)

# Hardware requirements   
- M5Stack   

- PX4 with WiFi Bridge   
You can use [this](https://github.com/dogmaphobic/mavesp8266) as Bridge.   

# Software requirements   
- ESP-IDF   

# Install
```
git clone https://github.com/nopnop2002/esp-idf-px4
cd esp-idf-mqtt-px4
mkdir -p components
cd components/
git clone https://github.com/mavlink/c_library_v2
cd c_library_v2/
lecho "COMPONENT_ADD_INCLUDEDIRS=." > component.mk
cd ../..
make menuconfig
make flash
```
![config-1](https://user-images.githubusercontent.com/6020549/95003113-b0c9e280-0616-11eb-8393-7b4c58f7b958.jpg)

![config-2](https://user-images.githubusercontent.com/6020549/95003115-b4f60000-0616-11eb-914d-1baa9494a65e.jpg)


# Firmware configuration
You have to set this config value using menuconfig.   

- CONFIG_ESP_WIFI_SSID   
SSID of your wifi.
- CONFIG_ESP_WIFI_PASSWORD   
PASSWORD of your wifi.
- CONFIG_ESP_MAXIMUM_RETRY   
Maximum number of retries when connecting to wifi.
- CONFIG_UDP_PORT   
Port number of PX4 MAVLink UDP.
- CONFIG_BAD_CRC   
Display packets with CRC error for debug.
- CONFIG_ESP_FONT   
The font to use.

# Operation

## General Infomation
Initial screen.   
Press Left button briefly.   
Left:Gothic font.   
Right:Mincyo font.   
![general](https://user-images.githubusercontent.com/6020549/95003118-ba534a80-0616-11eb-9d65-ecf0559e6e51.JPG)

## Heading Information
Press Middle button briefly.   
![heading](https://user-images.githubusercontent.com/6020549/95003119-bcb5a480-0616-11eb-86af-ea6cab5fd160.JPG)

## Speed Information
Press Right button briefly.   
![speed](https://user-images.githubusercontent.com/6020549/95003281-c0e2c180-0618-11eb-8d41-bd8693f762f6.JPG)

