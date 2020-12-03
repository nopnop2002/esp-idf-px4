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
	char senderstr[16];
	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

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
		inet_ntop(AF_INET, &senderInfo.sin_addr, senderstr, sizeof(senderstr));
		ESP_LOGD(TAG,"recvfrom : %s, port=%d", senderstr, ntohs(senderInfo.sin_port));

		uint8_t msgReceived;
/*
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
*/
		mavlink_message_t _rxmsg;
		mavlink_status_t  _rxstatus;
		mavlink_message_t _message;
		mavlink_status_t  _mav_status;
		for (int index=0; index<ret; index++) {
			uint8_t result = buffer[index];
			msgReceived = mavlink_frame_char_buffer(&_rxmsg, &_rxstatus, result, &_message, &_mav_status);
			ESP_LOGD(TAG,"msgReceived=%d", msgReceived);
			if (msgReceived == 1) {
				ESP_LOGD(TAG,"_message.msgid=%d", _message.msgid);

				if (_message.sysid == 1 && _message.compid == 240) {
					ESP_LOGI(TAG,"sysid=%d compid=%d seq=%d msgid=%d",_message.sysid, _message.compid, _message.seq, _message.msgid);
				}

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_RADIO_STATUS) {
					ESP_LOGD(TAG,"RADIO_STATUS");
					ESP_LOGD(TAG,"sysid=%d compid=%d seq=%d",_message.sysid, _message.compid, _message.seq);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_RADIO_STATUS) {
					ESP_LOGI(TAG,"RADIO_STATUS");
					ESP_LOGI(TAG,"sysid=%d compid=%d seq=%d",_message.sysid, _message.compid, _message.seq);
				}

				if (_message.msgid ==  MAVLINK_MSG_ID_HEARTBEAT) {
					ESP_LOGI(TAG,"HEARTBEAT");
					ESP_LOGI(TAG,"sysid=%d compid=%d seq=%d",_message.sysid, _message.compid, _message.seq);
				}

				if (_message.msgid ==  MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
					ESP_LOGI(TAG,"PARAM_REQUEST_LIST");
					ESP_LOGI(TAG,"sysid=%d compid=%d seq=%d",_message.sysid, _message.compid, _message.seq);
				}
				if (_message.msgid ==  MAVLINK_MSG_ID_BATTERY_STATUS) {
					mavlink_battery_status_t param;
					mavlink_msg_battery_status_decode(&_message, &param);
					ESP_LOGI(TAG,"BATTERY_STATUS");
					ESP_LOGI(TAG,"battery_remaining=%d voltages=%u", param.battery_remaining, (unsigned int)param.voltages);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
					mavlink_local_position_ned_t param;
					mavlink_msg_local_position_ned_decode(&_message, &param);
					ESP_LOGI(TAG,"LOCAL_POSITION_NED");
					ESP_LOGI(TAG,"x=%f y=%f z=%f", param.x, param.y, param.z);
					ESP_LOGI(TAG,"vx=%f vy=%f vz=%f", param.vx, param.vy, param.vz);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_ATTITUDE) {
					mavlink_attitude_t param;
					mavlink_msg_attitude_decode(&_message, &param);
					ESP_LOGI(TAG,"ATTITUDE");
					ESP_LOGI(TAG,"roll=%f pitch=%f yaw=%f", param.roll, param.pitch, param.yaw);
					ESP_LOGI(TAG,"rollspeed=%f pitchspeed=%f yawspeed=%f", param.rollspeed, param.pitchspeed, param.yawspeed);
				}
#endif

				if (_message.msgid ==  MAVLINK_MSG_ID_VFR_HUD) {
					mavlink_vfr_hud_t param;
					mavlink_msg_vfr_hud_decode(&_message, &param);
					ESP_LOGD(TAG,"VFR_HUD");
					ESP_LOGD(TAG,"sysid=%d compid=%d seq=%d",_message.sysid, _message.compid, _message.seq);
					ESP_LOGD(TAG,"airspeed=%f groundspeed=%f alt=%f", param.airspeed, param.groundspeed, param.alt);
					ESP_LOGD(TAG,"climb=%f heading=%d throttle=%d", param.climb, param.heading, param.throttle);
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
					ESP_LOGI(TAG,"HIGHRES_IMU");
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_BATTERY_STATUS) {
					mavlink_battery_status_t param;
					mavlink_msg_battery_status_decode(&_message, &param);
					ESP_LOGI(TAG,"BATTERY_STATUS");
					ESP_LOGI(TAG,"battery_remaining=%d voltages=%u", param.battery_remaining, (unsigned int)param.voltages);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
					mavlink_local_position_ned_t param;
					mavlink_msg_local_position_ned_decode(&_message, &param);
					ESP_LOGI(TAG,"LOCAL_POSITION_NED");
					ESP_LOGI(TAG,"x=%f y=%f z=%f", param.x, param.y, param.z);
					ESP_LOGI(TAG,"vx=%f vy=%f vz=%f", param.vx, param.vy, param.vz);
				}
#endif

#if 0
				if (_message.msgid ==  MAVLINK_MSG_ID_ATTITUDE) {
					mavlink_attitude_t param;
					mavlink_msg_attitude_decode(&_message, &param);
					ESP_LOGI(TAG,"ATTITUDE");
					ESP_LOGI(TAG,"roll=%f pitch=%f yaw=%f", param.roll, param.pitch, param.yaw);
					ESP_LOGI(TAG,"rollspeed=%f pitchspeed=%f yawspeed=%f", param.rollspeed, param.pitchspeed, param.yawspeed);
				}
#endif

				if (_message.msgid ==  MAVLINK_MSG_ID_VFR_HUD) {
					mavlink_vfr_hud_t param;
					mavlink_msg_vfr_hud_decode(&_message, &param);
					ESP_LOGD(TAG,"VFR_HUD");
					ESP_LOGD(TAG,"airspeed=%f groundspeed=%f alt=%f", param.airspeed, param.groundspeed, param.alt);
					ESP_LOGD(TAG,"climb=%f heading=%d throttle=%d", param.climb, param.heading, param.throttle);
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
					ESP_LOGI(TAG,"HIGHRES_IMU");
					ESP_LOGI(TAG,"xacc=%f yacc=%f zacc=%f", param.xacc, param.yacc, param.zacc);
					ESP_LOGI(TAG,"xgyro=%f ygyro=%f zgyro=%f", param.xgyro, param.ygyro, param.zgyro);
					ESP_LOGI(TAG,"xmag=%f ymag=%f zmag=%f", param.xmag, param.ymag, param.zmag);
				}
#endif

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