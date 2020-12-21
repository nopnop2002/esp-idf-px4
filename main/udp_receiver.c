/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include <ardupilotmega/mavlink.h>

#include "cmd.h"

extern QueueHandle_t xQueueCmd;

static const char *TAG = "UDP";

//#define CONFIG_BAD_CRC	0
//#define CONFIG_UDP_PORT	14550

// Bradcast Receive Task
void receiver(void *pvParameters)
{
	ESP_LOGI(TAG, "Start. Wait for %d port", CONFIG_UDP_PORT);

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
	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	mavlink_message_t _rxmsg;
	mavlink_status_t  _rxstatus;
	mavlink_message_t _message;
	mavlink_status_t  _mav_status;
	bzero(&_rxstatus, sizeof(mavlink_status_t));

	while(1) {
		socklen_t senderInfoLen = sizeof(senderInfo);
		memset(buffer, 0, sizeof(buffer));
		ret = lwip_recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&senderInfo, &senderInfoLen);
		LWIP_ASSERT("ret > 0", ret > 0);
		ESP_LOGD(TAG,"lwip_recv ret=%d",ret);
		if (ret == 0) continue;

		buffer[ret] = 0;
		//ESP_LOGI(TAG,"lwip_recv buffer=%s",buffer);
		ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, ret, ESP_LOG_DEBUG);
#if 0
		char senderstr[16];
		inet_ntop(AF_INET, &senderInfo.sin_addr, senderstr, sizeof(senderstr));
		ESP_LOGD(TAG,"recvfrom : %s, port=%d", senderstr, ntohs(senderInfo.sin_port));
#endif

#if 0
typedef struct __mavlink_message {
	uint16_t checksum;		///< sent at end of packet
	uint8_t magic;			///< protocol magic marker
	uint8_t len;			///< Length of payload
	uint8_t incompat_flags; ///< flags that must be understood
	uint8_t compat_flags;	///< flags that can be ignored if not understood
	uint8_t seq;			///< Sequence of packet
	uint8_t sysid;			///< ID of message sender system/aircraft
	uint8_t compid;			///< ID of the message sender component
	uint32_t msgid:24;		///< ID of message in payload
	uint64_t payload64[(MAVLINK_MAX_PAYLOAD_LEN+MAVLINK_NUM_CHECKSUM_BYTES+7)/8];
	uint8_t ck[2];			///< incoming checksum bytes
	uint8_t signature[MAVLINK_SIGNATURE_BLOCK_LEN];
}) mavlink_message_t;
#endif

		for (int index=0; index<ret; index++) {
			uint8_t result = buffer[index];
			uint8_t msgReceived = mavlink_frame_char_buffer(&_rxmsg, &_rxstatus, result, &_message, &_mav_status);
			ESP_LOGD(TAG,"msgReceived=%d", msgReceived);
			if (msgReceived == 1) {
				ESP_LOGD(TAG,"_message.msgid=%d _message.compid=%d", _message.msgid, _message.compid);

				if (_message.compid != 1) {
					ESP_LOGD(TAG,"sysid=%d compid=%d seq=%d msgid=%d",_message.sysid, _message.compid, _message.seq, _message.msgid);
					continue;
				}

				if (_message.msgid ==  MAVLINK_MSG_ID_VFR_HUD) {
					mavlink_vfr_hud_t param;
					mavlink_msg_vfr_hud_decode(&_message, &param);
					ESP_LOGI(TAG,"VFR_HUD:airspeed=%f groundspeed=%f alt=%f", param.airspeed, param.groundspeed, param.alt);
					ESP_LOGI(TAG,"VFR_HUD:climb=%f heading=%d throttle=%d", param.climb, param.heading, param.throttle);
					cmdBuf.command = CMD_MAVLINK;
					cmdBuf.airspeed = param.airspeed;
					cmdBuf.groundspeed = param.groundspeed;
					cmdBuf.alt = param.alt;
					cmdBuf.climb = param.climb;
					cmdBuf.heading = param.heading;
					cmdBuf.throttle = param.throttle;
					xQueueSend(xQueueCmd, &cmdBuf, 0);
				}

			} else if (msgReceived == 2) {
#if CONFIG_BAD_CRC
				ESP_LOGW(TAG,"BAD CRC");
				ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, ret, ESP_LOG_WARN);
#endif
			}
		}
	}

	/* close socket. Don't reach here. */
	ret = lwip_close(fd);
	LWIP_ASSERT("ret == 0", ret == 0);
	vTaskDelete( NULL );
}
