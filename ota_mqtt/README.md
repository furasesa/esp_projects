# SMART CONFIG + MQTT + OTA UPDATEE

* <b>Smart Config</b> : Is easy tool to broadcast SSID and Password to ESPs.
* <b>MQTT TCP</b>: Centralized control uses MOSQUITTO BROKER to control your rpi or esp. Mosquitto Version v.3.1.1
* <b> OTA Updater</b>: Update your ESP/s via wireless with http/s protocol.

## IMPORTANT NOTICE (SDK)

- ESP32  	: ESP_IDF 4.3.1
- MQTT		: MQTT v3.1.1 Broker
- Mosquitto	: mosquitto version 1.5.7

Please check your MQTT Broker and ESP client version is match. Set it in menuconfig


## USER_CONFIGURATION

set your idf_path

```
example:
source /path_to_your_idf/esp-idf-v4.3.1/export.sh
idf.py menuconfig

```

from menuconfig you can also change custom partition.
it uses partition config by default (2 ota).

Skip Using CONFIG from sdkconfig because of building time.

```
//#define SMARTCONFIG_TYPE       CONFIG_ESP_SMARTCONFIG_TYPE
//#define BROKER_URL             CONFIG_BROKER_URL
//#define DEVICE_NAME            CONFIG_DEVICE_NAME
//#define FIRMWARE_UPGRADE_URL   CONFIG_FIRMWARE_UPGRADE_URL

#define SMARTCONFIG_TYPE         SC_TYPE_ESPTOUCH_V2
#define BROKER_URL               "mqtt://192.168.1.200"
#define DEVICE_NAME              "UNIQUE_NAME"
#define FIRMWARE_UPGRADE_URL     "http://192.168.1.200:8070/ota_latest.bin"
```

## SETUP YOUR GPIO

```
// INPUT
#define GPIO_INPUT_T0	4
#define GPIO_INPUT_T7	27
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_T0) | (1ULL<<GPIO_INPUT_T7))

// OUTPUT
#define LED_BUILT_IN	2
#define GPIO_OUTPUT_RX2	16
#define GPIO_OUTPUT_TX2	17
#define GPIO_OUTPUT_D05	5
#define GPIO_OUTPUT_D18	18
#define GPIO_OUTPUT_D19	19
#define GPIO_OUTPUT_D21	21
#define GPIO_OUTPUT_D22	22
#define GPIO_OUTPUT_D23	23
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<LED_BUILT_IN) | (1ULL<<GPIO_OUTPUT_RX2) | (1ULL<<GPIO_OUTPUT_RX2) | (1ULL<<GPIO_OUTPUT_TX2) | (1ULL<<GPIO_OUTPUT_D05) | (1ULL<<GPIO_OUTPUT_D18) | (1ULL<<GPIO_OUTPUT_D19) | (1ULL<<GPIO_OUTPUT_D21) | (1ULL<<GPIO_OUTPUT_D22) | (1ULL<<GPIO_OUTPUT_D23))
```

## BUILD PROJECT & FLASH

```
idf.py build
idf.py -p /dev/ttyUSB? flash monitor
```

## FLASH Via OTA

```
idf.py build

```

goto build/ folder. copy ota_mqtt.bin to your http path folder and rename it.

```
mkdir -p build/publish
cp build/ota_mqtt.bin build/publish/<your_ota_name>.bin // check FIRMWARE_UPGRADE_URL
cd publish
python3 -m http.server 8070

# to run OTA update
mosquitto_pub <YOUR_UNIQUE_NAME>/ota/1
```

