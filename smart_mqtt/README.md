# SMART MQTT

This is <b>script kiddie</b>, it mean I use "copy-paste" from ESP-IDF.
smart_mqtt is implement of <b>Smart Config</b> and <b>MQTT TCP</b> to control GPIO.

* <b>Smart Config</b> : Is easy tool to broadcast SSID and Password to ESPs.
* <b>MQTT TCP</b>: Centralized control uses MOSQUITTO BROKER to control your rpi or esp.

It works for esp8266 and esp32 (uses different SDK).

## IMPORTANT NOTICE (SDK)
- ESP8266: ESP8266_RTOS_SDK : v3.4-49-g0f200b46
- ESP32  : ESP_IDF 4.3.1

It might be broken for newer SDK.

## GUIDE
* User Configuration (sdkconfig)
* Build, Flash, and Monitor

Some of them only test code between 2 ESP, you can ignore or remove them.

You can do some search -> TODO: CHANGE_IT


```
/*
 * TODO: CHANGE_IT
 * .....
 * 
 * // ESP32
 * # Some code
 * ......
 * // ESP8266
 * ......
 */
 
```

now goto <b>static void initialise_wifi(void)</b>. Search it -> TODO: CHANGE_IT WIFI_INIT

```
# if you flashing esp32
esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
assert(sta_netif);

# if you flashing esp8266 (disable it)
//esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
//assert(sta_netif);
```


### Configure the project

<b>set your SDK</b>

```
# for ESP8266
cd ESP8266_RTOS_SDK/
source export.sh

# for ESP32
cd esp-idf-v4.3.1

# goto project source
cd smart_mqtt/
make menuconfig
# or
idf.py menuconfig
```

## USER_CONFIGURATION

You can see <b>Kconfig.projbuild</b>. it has 3 config.

1. ESP_SMARTCONFIG_TYPE
2. BROKER_URL
3. DEVICE_NAME

Changing sdkconfig will perform fullbuild, it mean requires more time. You can skip it by enum

```
#define SMARTCONFIG_TYPE		CONFIG_ESP_SMARTCONFIG_TYPE
#define BROKER_URL			CONFIG_BROKER_URL
#define DEVICE_NAME			CONFIG_DEVICE_NAME
```


### ESP_SMARTCONFIG_TYPE

	* ESP_TOUCH V1
	* AIRKISS
	* ESP_TOUCH_AIRKISS
	* ESP_TOUCH_V2 (recommended)

Download ESPTOUCH APP from app store. [Android source code](https://github.com/EspressifApp/EsptouchForAndroid) and [iOS source code](https://github.com/EspressifApp/EsptouchForIOS) is available.

### BROKER_URL

	Cannot detect local avahi config like *.localhost. Please use IP Number instead. e.g: mqtt://192.168.1.150
	Save sdkconfig and exit

### DEVICE_NAME

It is mandatory because it uses for mqtt unique address. Desired template:
CONFIG_DEVICE_NAME/GPIO/

example topic:
ESP8266/D0/

set payload data to string with value 0 or 1.

please make sure to end with '/' because of <b>carriage return</b> like : ESP8266/d5�?��"@���?


### Now you can build or flash
```
# build only
idf.py build
# build, flash and monitor
idf.py -p PORT flash monitor
```
