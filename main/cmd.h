#define CMD_BUTTON_LEFT		100
#define CMD_BUTTON_MIDDLE	200
#define CMD_BUTTON_RIGHT	300
#define CMD_MAVLINK			400

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
