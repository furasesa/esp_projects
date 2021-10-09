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
#include "nvs_flash.h"

// SMART CONFIG
// deprecated on esp32 uses esp_netif.h instead
//#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

// MQTT TCP
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
 */
#define SMARTCONFIG_TYPE	CONFIG_ESP_SMARTCONFIG_TYPE
#define BROKER_URL			CONFIG_BROKER_URL
#define DEVICE_NAME			CONFIG_DEVICE_NAME



/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

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
 *
 */
/*
 * TODO: CHANGE_IT
 * ie defined gpio. please see pinout folder
 */
// ESP32
// INPUT
 #define GPIO_INPUT_T0	4
 #define GPIO_INPUT_T2	2
 #define GPIO_INPUT_T7	27
 #define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_T0) | (1ULL<<GPIO_INPUT_T2) | (1ULL<<GPIO_INPUT_T7))

 // OUTPUT
 #define GPIO_OUTPUT_RX2	16
 #define GPIO_OUTPUT_TX2	17
 #define GPIO_OUTPUT_D05	5
 #define GPIO_OUTPUT_D18	18
 #define GPIO_OUTPUT_D19	19
 #define GPIO_OUTPUT_D21	21
 #define GPIO_OUTPUT_D22	22
 #define GPIO_OUTPUT_D23	23
 #define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_RX2) | (1ULL<<GPIO_OUTPUT_TX2) | (1ULL<<GPIO_OUTPUT_D05) | (1ULL<<GPIO_OUTPUT_D18) | (1ULL<<GPIO_OUTPUT_D19) | (1ULL<<GPIO_OUTPUT_D21) | (1ULL<<GPIO_OUTPUT_D22) | (1ULL<<GPIO_OUTPUT_D23))


//ESP8266
// INPUT
//#define GPIO_INPUT_D0	16
//#define GPIO_INPUT_D1	5
//#define GPIO_INPUT_D2	4
//#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_D0) | (1ULL<<GPIO_INPUT_D1) | (1ULL<<GPIO_INPUT_D2))
//
//// OUTPUT
//#define GPIO_OUTPUT_D5	14
//#define GPIO_OUTPUT_D6	12
//#define GPIO_OUTPUT_D7	13
//#define GPIO_OUTPUT_D8	15
//#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_D5) | (1ULL<<GPIO_OUTPUT_D6) | (1ULL<<GPIO_OUTPUT_D7) | (1ULL<<GPIO_OUTPUT_D8))

// END_CHANGE

static const char* TAG = "SMART_MQTT";

static void smartconfig_task(void* parm);


// MQTT
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;   

    char mqtt_topic[20];
    char mqtt_gpio_data[5];
    char dev_con[30];

    /*
     * TODO: CHANGE_IT
     */
//    ESP32
	char mqtt_rx2[20];
	char mqtt_tx2[20];
	char mqtt_d05[20];
	char mqtt_d18[20];
	char mqtt_d19[20];
	char mqtt_d21[20];
	char mqtt_d22[20];
	char mqtt_d23[20];

	strcpy(dev_con, DEVICE_NAME);
	strcpy(mqtt_rx2, DEVICE_NAME);
	strcpy(mqtt_tx2, DEVICE_NAME);
	strcpy(mqtt_d05, DEVICE_NAME);
	strcpy(mqtt_d18, DEVICE_NAME);
	strcpy(mqtt_d19, DEVICE_NAME);
	strcpy(mqtt_d21, DEVICE_NAME);
	strcpy(mqtt_d22, DEVICE_NAME);
	strcpy(mqtt_d23, DEVICE_NAME);

	strcat(dev_con, "/connection/");
	strcat(mqtt_rx2, "/rx2/");
	strcat(mqtt_tx2, "/tx2/");
	strcat(mqtt_d05, "/d05/");
	strcat(mqtt_d18, "/d18/");
	strcat(mqtt_d19, "/d19/");
	strcat(mqtt_d21, "/d21/");
	strcat(mqtt_d22, "/d22/");
	strcat(mqtt_d23, "/d23/");

    // ESP8266
//    char mqtt_d05[20];
//    char mqtt_d06[20];
//    char mqtt_d07[20];
//    char mqtt_d08[20];
//
//    strcpy(dev_con, DEVICE_NAME);
//    strcpy(mqtt_d05, DEVICE_NAME);
//    strcpy(mqtt_d06, DEVICE_NAME);
//    strcpy(mqtt_d07, DEVICE_NAME);
//    strcpy(mqtt_d08, DEVICE_NAME);
//
//    strcat(dev_con, "/connection/");
//    strcat(mqtt_d05, "/d05/");
//    strcat(mqtt_d06, "/d06/");
//    strcat(mqtt_d07, "/d07/");
//    strcat(mqtt_d08, "/d08/");

//    printf("dev_name: %s\ndev_con: %s\n", devn, dev_con);
//    printf("GPIO on MQTT:\nmqtt_d0:%s\n", mqtt_d0);


    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
//            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//
//            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
//            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//
//            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
//            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//
//            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
//            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            // TODO: Send topic to broker if device is connected
            msg_id = esp_mqtt_client_publish(client, dev_con, "1", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

			// Subscriber
            // ESP32
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

            // ESP8266
//			msg_id = esp_mqtt_client_subscribe(client, mqtt_d05, 1);
//			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_subscribe(client, mqtt_d06, 1);
//			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_subscribe(client, mqtt_d07, 1);
//			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_subscribe(client, mqtt_d08, 1);
//			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            // TODO: Disconnect indicator Output pin
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
//            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
//            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);


            // ESP32
			msg_id = esp_mqtt_client_publish(client, mqtt_rx2, "0", 0, 0, 0); //QOS0
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_tx2, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d05, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d18, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d19, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d21, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d22, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_publish(client, mqtt_d23, "0", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            // ESP8266
//			msg_id = esp_mqtt_client_publish(client, mqtt_d05, "0", 0, 0, 0);
//			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_publish(client, mqtt_d06, "0", 0, 0, 0);
//			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_publish(client, mqtt_d07, "0", 0, 0, 0);
//			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
//			msg_id = esp_mqtt_client_publish(client, mqtt_d08, "0", 0, 0, 0);
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


            	// ESP32
             	if (strcmp(token, "rx2")==0)
             	{
             		strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
             		printf("%s/RX2 : %d\n", DEVICE_NAME, gpio_data);
             		gpio_set_level(GPIO_OUTPUT_RX2, gpio_data);
             	}
             	else if (strcmp(token, "tx2")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/TX2 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_TX2, gpio_data);
 				}
             	else if (strcmp(token, "d05")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D05 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D05, gpio_data);
 				}
             	else if (strcmp(token, "d18")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D18 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D18, gpio_data);
 				}
             	else if (strcmp(token, "d19")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D19 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D19, gpio_data);
 				}
             	else if (strcmp(token, "d21")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D21 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D21, gpio_data);
 				}
             	else if (strcmp(token, "d22")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D22 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D22, gpio_data);
 				}
             	else if (strcmp(token, "d23")==0)
 				{
 					strncpy(mqtt_gpio_data, event->data, event->data_len);
 					int gpio_data = atoi(mqtt_gpio_data);
 					printf("%s/D23 : %d\n", DEVICE_NAME, gpio_data);
 					gpio_set_level(GPIO_OUTPUT_D23, gpio_data);
 				}

                // ESP8266
//            	if (strcmp(token, "d05")==0)
//            	{
//            		strncpy(mqtt_gpio_data, event->data, event->data_len);
//					int gpio_data = atoi(mqtt_gpio_data);
//            		printf("%s/D5 : %d\n", DEVICE_NAME, gpio_data);
//            		gpio_set_level(GPIO_OUTPUT_D5, gpio_data);
//            	}
//            	else if (strcmp(token, "d06")==0)
//				{
//					strncpy(mqtt_gpio_data, event->data, event->data_len);
//					int gpio_data = atoi(mqtt_gpio_data);
//					printf("%s/D6 : %d\n", DEVICE_NAME, gpio_data);
//					gpio_set_level(GPIO_OUTPUT_D6, gpio_data);
//				}
//            	else if (strcmp(token, "d07")==0)
//				{
//					strncpy(mqtt_gpio_data, event->data, event->data_len);
//					int gpio_data = atoi(mqtt_gpio_data);
//					printf("%s/D7 : %d\n", DEVICE_NAME, gpio_data);
//					gpio_set_level(GPIO_OUTPUT_D7, gpio_data);
//				}
//            	else if (strcmp(token, "d08")==0)
//				{
//					strncpy(mqtt_gpio_data, event->data, event->data_len);
//					int gpio_data = atoi(mqtt_gpio_data);
//					printf("%s/D8 : %d\n", DEVICE_NAME, gpio_data);
//					gpio_set_level(GPIO_OUTPUT_D8, gpio_data);
//				}

            	// endch
            	else {
            		printf("Unknown GPIO topic: %s\n",  event->topic);
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



// SMART CONFIG
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
        // TODO: MQTT START
        mqtt_app_start();
    }
}

static void initialise_wifi(void)
{
//    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /*
     * TODO: CHANGE_IT WIFI_INIT
     * ESP32 : ENABLE
     * ESP8266 : DISABLE
     */
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	// TO HERE

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_start() );
}

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


void app_main()
{
	// GPIO SEtting
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

	/*
	 * TODO: CHANGE_IT
	 */
	//change gpio intrrupt type for one pin
	// ESP32
	 gpio_set_intr_type(GPIO_INPUT_T0, GPIO_INTR_ANYEDGE);
    // ESP8266
//    gpio_set_intr_type(GPIO_INPUT_D0, GPIO_INTR_ANYEDGE);

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // MQTT LOG SET
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	// INIT
    ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();

    // GPIO
    //create a queue to handle gpio event from isr
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	//start gpio task
	xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

	//install gpio isr service
	gpio_install_isr_service(0);
	//hook isr handler for specific gpio pin


	/*
	 * TODO: CHANGE_IT
	 */
	// ESP32
	gpio_isr_handler_add(GPIO_INPUT_T0, gpio_isr_handler, (void *) GPIO_INPUT_T0);
	gpio_isr_handler_add(GPIO_INPUT_T2, gpio_isr_handler, (void *) GPIO_INPUT_T2);
	gpio_isr_handler_add(GPIO_INPUT_T7, gpio_isr_handler, (void *) GPIO_INPUT_T7);

    // ESP8266
//	gpio_isr_handler_add(GPIO_INPUT_D0, gpio_isr_handler, (void *) GPIO_INPUT_D0);
//	gpio_isr_handler_add(GPIO_INPUT_D1, gpio_isr_handler, (void *) GPIO_INPUT_D1);
//	gpio_isr_handler_add(GPIO_INPUT_D2, gpio_isr_handler, (void *) GPIO_INPUT_D2);

}

