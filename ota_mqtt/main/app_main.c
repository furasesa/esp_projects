// C
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

// FREERTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// ESP
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"

// ESP OTA HTTP client
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

// ESP SMART CONFIG
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

// ESP MQTT TCP
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"

// GPIO
#include "driver/gpio.h"

/*
 * TODO: CHANGE_IT
 * SMARTCONFIG_TYPE can set via sdkconfig -> user configuration menu
 * setting bellow is faster than via sdkconfig. ie:
 * #define SMARTCONFIG_TYPE SC_TYPE_ESPTOUCH
 * Values:
 * SC_TYPE_ESPTOUCH = 0 protocol: ESPTouch
 * SC_TYPE_AIRKISS protocol: AirKiss
 * SC_TYPE_ESPTOUCH_AIRKISS protocol: ESPTouch and AirKiss
 * SC_TYPE_ESPTOUCH_V2 protocol: ESPTouch v2
 *
 */

//#define SMARTCONFIG_TYPE		CONFIG_ESP_SMARTCONFIG_TYPE
//#define BROKER_URL				CONFIG_BROKER_URL
//#define DEVICE_NAME				CONFIG_DEVICE_NAME
//#define FIRMWARE_UPGRADE_URL	CONFIG_FIRMWARE_UPGRADE_URL

#define SMARTCONFIG_TYPE		SC_TYPE_ESPTOUCH_V2
#define BROKER_URL				"mqtt://192.168.1.200"
#define DEVICE_NAME				"SILVER0"
#define FIRMWARE_UPGRADE_URL	"http://192.168.1.200:8070/silver0_ota.bin"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/*
 * ESP32 DevKit ESP32-WROOM
 *
 * SPI		MOSI	MISO	CLK		CS
 * VSPI		23		19		18		5
 * HSPI		13		12		14		15
 *
 * I2C		SDA		SCL
 * 			21		22
 *
 * Serial 	RX		TX
 * U0		3		1
 * U2		16		17
 *
 * TOUCH	T0	T2	T3	T4	T5	T6	T7	T8	T9
 * TOUCH	4	2	15	13	12	14	27	33	32
 *
 * INPUT_PULLUP: 14, 16, 17, 18, 19, 21, 22, 23
 *
 * INPUT ONLY: 34, 35, 36, 39
 *
 * ESP32-WROOM : LED_BUILT_IN 2
 */
/*
 * TODO: CHANGE_IT
 * ie defined gpio. please see pinout folder
 */

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

#define HASH_LEN 32

static const char *TAG = DEVICE_NAME;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

// HTTP OTA
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void simple_ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
        .url = FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}
// HTTP OTA END



// MQTT
/*
 * mqtt_event_handler
 * TODO:
 * control GPIO
 * used to flash OTA
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;

    char mqtt_topic[20];
    char dev_con[30];

    // MQTT_PIN
	char mqtt_rx2[20];
	char mqtt_tx2[20];
	char mqtt_d05[20];
	char mqtt_d18[20];
	char mqtt_d19[20];
	char mqtt_d21[20];
	char mqtt_d22[20];
	char mqtt_d23[20];

	char mqtt_led[20];
	char mqtt_ota[20];

	strcpy(dev_con, DEVICE_NAME);
	strcpy(mqtt_ota, DEVICE_NAME);
	strcpy(mqtt_led, DEVICE_NAME);

	strcpy(mqtt_rx2, DEVICE_NAME);
	strcpy(mqtt_tx2, DEVICE_NAME);
	strcpy(mqtt_d05, DEVICE_NAME);
	strcpy(mqtt_d18, DEVICE_NAME);
	strcpy(mqtt_d19, DEVICE_NAME);
	strcpy(mqtt_d21, DEVICE_NAME);
	strcpy(mqtt_d22, DEVICE_NAME);
	strcpy(mqtt_d23, DEVICE_NAME);

	strcat(dev_con, "/connection/");
	strcat(mqtt_ota, "/ota/");
	strcat(mqtt_led, "/led/");

	strcat(mqtt_rx2, "/rx2/");
	strcat(mqtt_tx2, "/tx2/");
	strcat(mqtt_d05, "/d05/");
	strcat(mqtt_d18, "/d18/");
	strcat(mqtt_d19, "/d19/");
	strcat(mqtt_d21, "/d21/");
	strcat(mqtt_d22, "/d22/");
	strcat(mqtt_d23, "/d23/");

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_publish(client, dev_con, "1", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

			// Subscriber
			msg_id = esp_mqtt_client_subscribe(client, mqtt_rx2, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_tx2, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d05, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d18, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d19, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d21, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d22, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, mqtt_d23, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

			// Listen For OTA Update Confirm
			msg_id = esp_mqtt_client_subscribe(client, mqtt_ota, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

			msg_id = esp_mqtt_client_subscribe(client, mqtt_led, 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            // TODO: Disconnect indicator Output pin
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
//            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
//            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            // TODO: INPUT Publish data
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
//            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
//            printf("DATA=%.*s\r\n", event->data_len, event->data);
//            strcpy (mqtt_topic, event->topic);

//            printf("topic len : %d\ndata len : %d\n", event->topic_len, event->data_len);
            strncpy(mqtt_topic, event->topic, event->topic_len);
//            printf("mqtt topic : %s\nlen : %d\n", mqtt_topic, strlen(mqtt_topic));

            char *token = strtok(mqtt_topic, "/");
//            printf("initial token result: %s\n", token);

            if (strcmp(token, DEVICE_NAME) == 0)
            {

            	token = strtok(NULL, "/");
            	// new
            	if (strcmp(token, "d05")==0){
//					int gpio_data = atoi(event->data);
					printf("%s/D05 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D05, atoi(event->data));
				}
            	else if (strcmp(token, "d18")==0){
            		printf("%s/D18 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D18, atoi(event->data));
            	}
            	else if (strcmp(token, "d19")==0){
            		printf("%s/D19 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D19, atoi(event->data));
				}
            	else if (strcmp(token, "d21")==0){
					printf("%s/D21 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D21, atoi(event->data));
				}
            	else if (strcmp(token, "d22")==0){
					printf("%s/D22 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D22, atoi(event->data));
				}
            	else if (strcmp(token, "d23")==0){
					printf("%s/D23 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_D23, atoi(event->data));
				}
            	else if (strcmp(token, "rx2")==0){
					printf("%s/RX2 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_RX2, atoi(event->data));
				}
            	else if (strcmp(token, "tx2")==0){
					printf("%s/TX2 : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(GPIO_OUTPUT_TX2, atoi(event->data));
				}
            	else if (strcmp(token, "led")==0){
					printf("%s/LED : %d\n", DEVICE_NAME, atoi(event->data));
					gpio_set_level(LED_BUILT_IN, atoi(event->data));
				}
            	else if (strcmp(token, "ota")==0){
					printf("%s/ota : %d\n", DEVICE_NAME, atoi(event->data));
					xTaskCreate(&simple_ota_task, "ota_task", 8192, NULL, 5, NULL);
				}

            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER_URL,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}
// MQTT END



// SMART CONFIG
static void smartconfig_task(void* parm);
static void smartconfig_task(void* parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SMARTCONFIG_TYPE));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);

        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }

        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

// start MQTT
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t* evt = (smartconfig_event_got_ssid_pswd_t*)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;

        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:%s", rvd_data);
        }

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
        // TODO: mqtt_start
        mqtt_app_start();
    }
}

static void initialise_wifi(void)
{
//    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_start() );
}
// SMART CONFIG END



// GPIO
static xQueueHandle gpio_evt_queue = NULL;

static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void *arg)
{
    uint32_t io_num;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}


void app_main(void)
{
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    get_sha256_of_partitions();

    // GPIO INIT
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO15/16
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);

	//interrupt of rising edge
	io_conf.intr_type = GPIO_INTR_POSEDGE;
	//bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	//set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	//enable pull-up mode
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);

	//change gpio intrrupt type for one pin
	gpio_set_intr_type(GPIO_INPUT_T0, GPIO_INTR_ANYEDGE);


    // MQTT LOG SET
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);


	initialise_wifi();

	// GPIO
	//create a queue to handle gpio event from isr
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	//start gpio task
	xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

	//install gpio isr service
	gpio_install_isr_service(0);
	//hook isr handler for specific gpio pin

	gpio_isr_handler_add(GPIO_INPUT_T0, gpio_isr_handler, (void *) GPIO_INPUT_T0);
//	gpio_isr_handler_add(GPIO_INPUT_T2, gpio_isr_handler, (void *) GPIO_INPUT_T2);
	gpio_isr_handler_add(GPIO_INPUT_T7, gpio_isr_handler, (void *) GPIO_INPUT_T7);
}
