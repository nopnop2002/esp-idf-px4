/* Metrics typically displayed on a HUD for M5Stack

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
//#include "lwip/dns.h

#include <ardupilotmega/mavlink.h>

#include "ili9340.h"
#include "fontx.h"

// for M5Stack
#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240
#define CS_GPIO			14
#define DC_GPIO			27
#define RESET_GPIO		33
#define BL_GPIO			32
#define DISPLAY_LENGTH	26
#define GPIO_INPUT_A	GPIO_NUM_39
#define GPIO_INPUT_B	GPIO_NUM_38
#define GPIO_INPUT_C	GPIO_NUM_37

// for Queue
typedef struct {
	uint16_t command;
	float airspeed; /*< Current airspeed in m/s*/
	float groundspeed; /*< Current ground speed in m/s*/
	float alt; /*< Current altitude (MSL), in meters*/
	float climb; /*< Current climb rate in meters/second*/
	int16_t heading; /*< Current heading in degrees, in compass units (0..360, 0=north)*/
	uint16_t throttle; /*< Current throttle setting in integer percent, 0 to 100*/
	TaskHandle_t taskHandle;
} CMD_t;
static QueueHandle_t xQueueCmd;

#define CMD_BUTTON_LEFT		100
#define CMD_BUTTON_MIDDLE	200
#define CMD_BUTTON_RIGHT	300
#define CMD_MAVLINK			400

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID	   CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS	   CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT	   BIT1

static const char *TAG = "main";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

//void wifi_init_sta(void)
esp_err_t wifi_init_sta(void)
{
	esp_err_t ret_value = ESP_OK;
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID,
			&event_handler,
			NULL,
			&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			IP_EVENT_STA_GOT_IP,
			&event_handler,
			NULL,
			&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.password = EXAMPLE_ESP_WIFI_PASS,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
		 .threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
				 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_FAIL;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	return ret_value;
}

//#define CONFIG_BAD_CRC	0
//#define CONFIG_UDP_PORT	14550

// Bradcast Receive Task
void receiver(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start. Wait for %d port", CONFIG_UDP_PORT);

	/* set up address to recvfrom */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_UDP_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY); /* senderInfo message from ANY */

	/* create the socket */
	int fd;
	int ret;
	fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.
	LWIP_ASSERT("fd >= 0", fd >= 0);

	#if 0
	/* set option */
	int broadcast=1;
	ret = lwip_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);
	LWIP_ASSERT("ret >= 0", ret >= 0);
	#endif

	/* bind socket */
	ret = lwip_bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	LWIP_ASSERT("ret >= 0", ret >= 0);

	/* senderInfo data */
	char buffer[128];
	struct sockaddr_in senderInfo;
	//socklen_t senderInfoLen = sizeof(senderInfo);
	char senderstr[16];
	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	while(1) {
		socklen_t senderInfoLen = sizeof(senderInfo);
		memset(buffer, 0, sizeof(buffer));
		ret = lwip_recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&senderInfo, &senderInfoLen);
		LWIP_ASSERT("ret > 0", ret > 0);
		ESP_LOGD(pcTaskGetTaskName(0),"lwip_recv ret=%d",ret);
		if (ret == 0) continue;

		buffer[ret] = 0;
		//ESP_LOGI(pcTaskGetTaskName(0),"lwip_recv buffer=%s",buffer);
		ESP_LOG_BUFFER_HEXDUMP(pcTaskGetTaskName(0), buffer, ret, ESP_LOG_DEBUG);
		inet_ntop(AF_INET, &senderInfo.sin_addr, senderstr, sizeof(senderstr));
		ESP_LOGD(pcTaskGetTaskName(0),"recvfrom : %s, port=%d", senderstr, ntohs(senderInfo.sin_port));

		uint8_t msgReceived;
		mavlink_message_t _rxmsg;
		mavlink_status_t  _rxstatus;
		mavlink_message_t _message;
		mavlink_status_t  _mav_status;
		for (int index=0; index<ret; index++) {
			uint8_t result = buffer[index];
			msgReceived = mavlink_frame_char_buffer(&_rxmsg, &_rxstatus, result, &_message, &_mav_status);
			ESP_LOGD(pcTaskGetTaskName(0),"msgReceived=%d", msgReceived);
			if (msgReceived == 1) {
				ESP_LOGD(pcTaskGetTaskName(0),"_message.msgid=%d", _message.msgid);

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
					mavlink_local_position_ned_t param;
					mavlink_msg_local_position_ned_decode(&_message, &param);
					ESP_LOGI(pcTaskGetTaskName(0),"LOCAL_POSITION_NED");
					ESP_LOGI(pcTaskGetTaskName(0),"x=%f y=%f z=%f", param.x, param.y, param.z);
					ESP_LOGI(pcTaskGetTaskName(0),"vx=%f vy=%f vz=%f", param.vx, param.vy, param.vz);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_ATTITUDE) {
					mavlink_attitude_t param;
					mavlink_msg_attitude_decode(&_message, &param);
					ESP_LOGI(pcTaskGetTaskName(0),"ATTITUDE");
					ESP_LOGI(pcTaskGetTaskName(0),"roll=%f pitch=%f yaw=%f", param.roll, param.pitch, param.yaw);
					ESP_LOGI(pcTaskGetTaskName(0),"rollspeed=%f pitchspeed=%f yawspeed=%f", param.rollspeed, param.pitchspeed, param.yawspeed);
				}
#endif

				if (_message.msgid ==  MAVLINK_MSG_ID_VFR_HUD) {
					mavlink_vfr_hud_t param;
					mavlink_msg_vfr_hud_decode(&_message, &param);
					ESP_LOGI(pcTaskGetTaskName(0),"VFR_HUD");
					ESP_LOGI(pcTaskGetTaskName(0),"airspeed=%f groundspeed=%f alt=%f", param.airspeed, param.groundspeed, param.alt);
					ESP_LOGI(pcTaskGetTaskName(0),"climb=%f heading=%d throttle=%d", param.climb, param.heading, param.throttle);
					cmdBuf.command = CMD_MAVLINK;
					cmdBuf.airspeed = param.airspeed;
					cmdBuf.groundspeed = param.groundspeed;
					cmdBuf.alt = param.alt;
					cmdBuf.climb = param.climb;
					cmdBuf.heading = param.heading;
					cmdBuf.throttle = param.throttle;
					xQueueSend(xQueueCmd, &cmdBuf, 0);
				}

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_HIGHRES_IMU) {
					mavlink_highres_imu_t param;
					mavlink_msg_highres_imu_decode(&_message, &param);
					ESP_LOGI(pcTaskGetTaskName(0),"HIGHRES_IMU");
					ESP_LOGI(pcTaskGetTaskName(0),"xacc=%f yacc=%f zacc=%f", param.xacc, param.yacc, param.zacc);
					ESP_LOGI(pcTaskGetTaskName(0),"xgyro=%f ygyro=%f zgyro=%f", param.xgyro, param.ygyro, param.zgyro);
					ESP_LOGI(pcTaskGetTaskName(0),"xmag=%f ymag=%f zmag=%f", param.xmag, param.ymag, param.zmag);
				}
#endif

			} else if (msgReceived == 2) {
#if CONFIG_BAD_CRC
				ESP_LOGW(pcTaskGetTaskName(0),"BAD CRC");
				ESP_LOG_BUFFER_HEXDUMP(pcTaskGetTaskName(0), buffer, ret, ESP_LOG_WARN);
#endif
			}
		}
	}

	/* close socket. Don't reach here. */
	ret = lwip_close(fd);
	LWIP_ASSERT("ret == 0", ret == 0);
	vTaskDelete( NULL );
}

// Left Button Monitoring
void buttonA(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_BUTTON_LEFT;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_pad_select_gpio(GPIO_INPUT_A);
	gpio_set_direction(GPIO_INPUT_A, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_A);
		if (level == 0) {
			ESP_LOGI(pcTaskGetTaskName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_A);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

// Middle Button Monitoring
void buttonB(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_BUTTON_MIDDLE;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_pad_select_gpio(GPIO_INPUT_B);
	gpio_set_direction(GPIO_INPUT_B, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_B);
		if (level == 0) {
			ESP_LOGI(pcTaskGetTaskName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_B);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

// Right Button Monitoring
void buttonC(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_BUTTON_RIGHT;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_pad_select_gpio(GPIO_INPUT_C);
	gpio_set_direction(GPIO_INPUT_C, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_C);
		if (level == 0) {
			ESP_LOGI(pcTaskGetTaskName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_C);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

//#define CONFIG_ESP_FONT_GOTHIC	1
//#define CONFIG_ESP_FONT_MINCYO	0

void tft(void *pvParameters)
{
	ESP_LOGI(pcTaskGetTaskName(0), "Start");
	// Set font file
	FontxFile fx[2];
#if CONFIG_ESP_FONT_GOTHIC
	InitFontx(fx,"/fonts/ILGH24XB.FNT",""); // 12x24Dot Gothic
#endif
#if CONFIG_ESP_FONT_MINCYO
	InitFontx(fx,"/fonts/ILMH24XB.FNT",""); // 12x24Dot Mincyo
#endif

	// Get font width & height
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGI(pcTaskGetTaskName(0), "fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	// Setup Screen
	TFT_t dev;
	spi_master_init(&dev, CS_GPIO, DC_GPIO, RESET_GPIO, BL_GPIO);
	lcdInit(&dev, 0x9341, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);
	ESP_LOGI(pcTaskGetTaskName(0), "Setup Screen done");

	int lines = (SCREEN_HEIGHT - fontHeight) / fontHeight;
	ESP_LOGD(pcTaskGetTaskName(0), "SCREEN_HEIGHT=%d fontHeight=%d lines=%d", SCREEN_HEIGHT, fontHeight, lines);
	int ymax = (lines+1) * fontHeight;
	ESP_LOGD(pcTaskGetTaskName(0), "ymax=%d",ymax);

	// Clear Screen
	lcdFillScreen(&dev, BLACK);
	lcdSetFontDirection(&dev, 0);

	// Reset scroll area
	lcdSetScrollArea(&dev, 0, 0x0140, 0);

	// Show header
	uint8_t ascii[44];
	uint16_t xpos = 0;
	uint16_t ypos = fontHeight-1;
	strcpy((char *)ascii, "PX4 HUD");
	lcdDrawString(&dev, fx, xpos, ypos, ascii, YELLOW);

	// Show sub title
	//uint16_t xTitle = fontWidth * 12 - 1;
	uint16_t xTitle = SCREEN_WIDTH/2;
	uint16_t yTitle = fontHeight-1;
	uint8_t subTitle[44];
	strcpy((char *)subTitle, "General Info");
	lcdDrawString(&dev, fx, xTitle, yTitle, subTitle, YELLOW);

	CMD_t cmdBuf;
	int screen = 1;

	// for general staff
	int16_t drawGeneral = 0;
	uint16_t xGeneral = (fontWidth * 14) - 1;

	// for heading staff
	int16_t drawHeading = 0;
	uint16_t xHeading = 0;
	uint16_t yHeading = 0;
	uint16_t headingRadius = 80;

	// for speed staff
	int16_t drawSpeed = 0;
	uint16_t xSpeed = 0;
	uint16_t ySpeed = 0;
	uint16_t speedRadius = 130;
#if 0
	int16_t airspeedPrimary = 0;
	int16_t airspeedDelta = 1;
#endif

	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetTaskName(0),"cmdBuf.command=%d screen=%d", cmdBuf.command, screen);
		if (cmdBuf.command == CMD_MAVLINK) {
			if (screen == 1){
				if (drawGeneral == 0) {
					lcdDrawFillRect(&dev, 0, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, BLACK);
					xpos = 0;
					ypos = (fontHeight*3)-1;
					strcpy((char *)ascii, "airspeed    : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "groundspeed : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "alt         : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "climb       : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "heading     : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "throttle    : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
				} else {
					// Erase Items
					lcdDrawFillRect(&dev, xGeneral, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, BLACK);
				}
				drawGeneral = 1;
				ypos = (fontHeight*3)-1;
				sprintf((char *)ascii, "%f", cmdBuf.airspeed);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				ypos = ypos + fontHeight;
				sprintf((char *)ascii, "%f", cmdBuf.groundspeed);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				ypos = ypos + fontHeight;
				sprintf((char *)ascii, "%f", cmdBuf.alt);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				ypos = ypos + fontHeight;
				sprintf((char *)ascii, "%f", cmdBuf.climb);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				ypos = ypos + fontHeight;
				sprintf((char *)ascii, "%d", cmdBuf.heading);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				ypos = ypos + fontHeight;
				sprintf((char *)ascii, "%d", cmdBuf.throttle);
				lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
			} else if (screen == 2) {
				uint16_t xCenter = SCREEN_WIDTH/2;
				uint16_t yCenter = (SCREEN_HEIGHT)/2 + (fontHeight/2);
				if (drawHeading == 0) {
					// Draw heading circle
					lcdDrawFillRect(&dev, 0, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, BLACK);
					lcdDrawCircle(&dev, xCenter, yCenter, headingRadius, CYAN);
					lcdDrawRect(&dev, 0, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, CYAN);

					// Draw Tick
					for(int deg=30;deg<360;deg=deg+30) {
						if ( (deg % 90) == 0) continue;
						float rad = deg * M_PI / 180.0;
						uint16_t xTick0 = xCenter + cos(rad) * (float)(headingRadius);
						uint16_t yTick0 = yCenter + sin(rad) * (float)(headingRadius);
						uint16_t xTick1 = xCenter + cos(rad) * (float)(headingRadius+10);
						uint16_t yTick1 = yCenter + sin(rad) * (float)(headingRadius+10);
						lcdDrawLine(&dev, xTick0, yTick0, xTick1, yTick1, CYAN);
					}

					uint16_t xLabel = SCREEN_WIDTH/2 - (fontWidth/2);
					uint16_t yLabel = (fontHeight * 2) -1;
					strcpy((char *)ascii, "N");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);
					yLabel = SCREEN_HEIGHT -1;
					strcpy((char *)ascii, "S");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);
					xLabel = 60;
					yLabel = yCenter;
					strcpy((char *)ascii, "W");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);
					xLabel = 250;
					strcpy((char *)ascii, "E");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);

				} else {
					// Erase Arrow
					lcdDrawArrow(&dev, xCenter, yCenter, xHeading, yHeading, 4, BLACK);
				}
				drawHeading = 1;

				// Draw Arrow
				int16_t heading = cmdBuf.heading - 90; 
				if (cmdBuf.heading < 90) heading = 270 + cmdBuf.heading;
				float rad = heading * M_PI / 180.0;
				xHeading = xCenter + cos(rad) * (float)(headingRadius-5);
				yHeading = yCenter + sin(rad) * (float)(headingRadius-5);
				lcdDrawArrow(&dev, xCenter, yCenter, xHeading, yHeading, 4, RED);

			} else if (screen == 3) {
				uint16_t xCenter = SCREEN_WIDTH/2;
				uint16_t yCenter = SCREEN_HEIGHT-20;
				if (drawSpeed == 0) {
					// Draw Circle
					lcdDrawFillRect(&dev, 0, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, BLACK);
					lcdDrawRect(&dev, 0, (fontHeight*1), SCREEN_WIDTH-1, SCREEN_HEIGHT-1, CYAN);

					// Draw Meter
					for(int deg=180;deg<=360;deg++) {
						float rad = deg * M_PI / 180.0;
						uint16_t xTick0 = xCenter + cos(rad) * (float)(speedRadius);
						uint16_t yTick0 = yCenter + sin(rad) * (float)(speedRadius);
						lcdDrawPixel(&dev, xTick0, yTick0, CYAN);
						uint16_t xTick1 = xCenter + cos(rad) * (float)(speedRadius+10);
						uint16_t yTick1 = yCenter + sin(rad) * (float)(speedRadius+10);
						lcdDrawPixel(&dev, xTick1, yTick1, CYAN);
					}
					for(int deg=180;deg<=360;deg=deg+45) {
						float rad = deg * M_PI / 180.0;
						uint16_t xTick0 = xCenter + cos(rad) * (float)(speedRadius);
						uint16_t yTick0 = yCenter + sin(rad) * (float)(speedRadius);
						uint16_t xTick1 = xCenter + cos(rad) * (float)(speedRadius+10);
						uint16_t yTick1 = yCenter + sin(rad) * (float)(speedRadius+10);
						lcdDrawLine(&dev, xTick0, yTick0, xTick1, yTick1, CYAN);
					}
					// Green zone
					for(int deg=180+90;deg<=180+90+45;deg++) {
						float rad = deg * M_PI / 180.0;
						uint16_t xTick0 = xCenter + cos(rad) * (float)(speedRadius);
						uint16_t yTick0 = yCenter + sin(rad) * (float)(speedRadius);
						uint16_t xTick1 = xCenter + cos(rad) * (float)(speedRadius+10);
						uint16_t yTick1 = yCenter + sin(rad) * (float)(speedRadius+10);
						lcdDrawLine(&dev, xTick0, yTick0, xTick1, yTick1, GREEN);
					}
					// Yellow zone
					for(int deg=180+90+45;deg<=360;deg++) {
						float rad = deg * M_PI / 180.0;
						uint16_t xTick0 = xCenter + cos(rad) * (float)(speedRadius);
						uint16_t yTick0 = yCenter + sin(rad) * (float)(speedRadius);
						uint16_t xTick1 = xCenter + cos(rad) * (float)(speedRadius+10);
						uint16_t yTick1 = yCenter + sin(rad) * (float)(speedRadius+10);
						lcdDrawLine(&dev, xTick0, yTick0, xTick1, yTick1, YELLOW);
					}
					uint16_t xLabel = 40;
					uint16_t yLabel = 110;
					strcpy((char *)ascii, "5");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);

					xLabel = (SCREEN_WIDTH / 2) - fontWidth;
					yLabel = 70;
					strcpy((char *)ascii, "10");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);

					xLabel = 250;
					yLabel = 110;
					strcpy((char *)ascii, "15");
					lcdDrawString(&dev, fx, xLabel, yLabel, ascii, CYAN);
				} else {
					// Erase Arrow
					lcdDrawArrow(&dev, xCenter, yCenter, xSpeed, ySpeed, 4, BLACK);
				}
				drawSpeed = 1;

#if 0
				// for debug
				cmdBuf.airspeed = airspeedPrimary;
				airspeedPrimary = airspeedPrimary + airspeedDelta;
				if (airspeedPrimary >= 20) airspeedDelta = -1;
				if (airspeedPrimary <= 0) airspeedDelta = 1;
#endif

				// Erase Speed
				xpos = SCREEN_WIDTH / 2 - fontWidth * 5;
				ypos = yCenter-(fontHeight*2);
				lcdDrawFillRect(&dev, xpos, ypos, xpos+fontWidth*10, ypos+fontHeight, BLACK);

				// Draw Speed
				sprintf((char *)ascii,"%4.1f m/Sec", cmdBuf.airspeed);
				xpos = SCREEN_WIDTH / 2 - fontWidth * 5;
				ypos = yCenter-(fontHeight*1);
				lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);

				// Draw Needle
				int16_t airspeed = cmdBuf.airspeed; 
				
				if (airspeed > 20) airspeed = 20;
				int16_t notched = 180 / 20;
				ESP_LOGI(pcTaskGetTaskName(0),"airspeed=%d", airspeed);
				airspeed = airspeed * notched + 180;
				float rad = airspeed * M_PI / 180.0;
				xSpeed = xCenter + cos(rad) * (float)(speedRadius-5);
				ySpeed = yCenter + sin(rad) * (float)(speedRadius-5);
				lcdDrawArrow(&dev, xCenter, yCenter, xSpeed, ySpeed, 4, RED);
			}

		} else if (cmdBuf.command == CMD_BUTTON_LEFT) {
			screen = 1;
			lcdDrawFillRect(&dev, xTitle, 0, SCREEN_WIDTH-1, fontHeight-1, BLACK);
			strcpy((char *)subTitle, "General Info");
			lcdDrawString(&dev, fx, xTitle, yTitle, subTitle, YELLOW);
			drawGeneral = 0; // Draw Frame
		} else if (cmdBuf.command == CMD_BUTTON_MIDDLE) {
			screen = 2;
			lcdDrawFillRect(&dev, xTitle, 0, SCREEN_WIDTH-1, fontHeight-1, BLACK);
			strcpy((char *)subTitle, "Heading Info");
			lcdDrawString(&dev, fx, xTitle, yTitle, subTitle, YELLOW);
			drawHeading = 0; // Draw Frame
		} else if (cmdBuf.command == CMD_BUTTON_RIGHT) {
			screen = 3;
			lcdDrawFillRect(&dev, xTitle, 0, SCREEN_WIDTH-1, fontHeight-1, BLACK);
			strcpy((char *)subTitle, "Speed Info");
			lcdDrawString(&dev, fx, xTitle, yTitle, subTitle, YELLOW);
			drawSpeed = 0; // Draw Frame
		}
	}

	// Don't reach here
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

static void SPIFFS_Directory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(TAG,"d_name=%s/%s d_ino=%d d_type=%x", path, pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

esp_err_t SPIFFS_Mount(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Mount %s to %s success", path, label);
		SPIFFS_Directory(path);
	}
	return ret;
}

void app_main(void)
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	  ESP_ERROR_CHECK(nvs_flash_erase());
	  ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initialize WiFi
	ESP_LOGI(TAG, "Initializing WiFi");
	if (wifi_init_sta() != ESP_OK) {
		ESP_LOGE(TAG, "Connection failed");
		while(1) { vTaskDelay(1); }
	}

	// Initialize SPIFFS
	ESP_LOGI(TAG, "Initializing SPIFFS");
	if (SPIFFS_Mount("/fonts", "storage", 6) != ESP_OK)
	{
		ESP_LOGE(TAG, "SPIFFS mount failed");
		while(1) { vTaskDelay(1); }
	}

	// Create Queue
	xQueueCmd = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueCmd );

	// Create Task
	xTaskCreate(receiver, "UDP", 1024*4, NULL, 2, NULL);
	xTaskCreate(buttonA, "BUTTON1", 1024*2, NULL, 2, NULL);
	xTaskCreate(buttonB, "BUTTON2", 1024*2, NULL, 2, NULL);
	xTaskCreate(buttonC, "BUTTON3", 1024*2, NULL, 2, NULL);
	xTaskCreate(tft, "TFT", 1024*8, NULL, 2, NULL);
}
