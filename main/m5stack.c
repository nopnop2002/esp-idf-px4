/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "driver/gpio.h"

#include "ili9340.h"
#include "fontx.h"
#include "cmd.h"

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

extern QueueHandle_t xQueueCmd;

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
	CMD_t cmdBufOld;
	cmdBufOld.airspeed = FLT_MAX;
	cmdBufOld.groundspeed = FLT_MAX;
	cmdBufOld.alt = FLT_MAX;
	cmdBufOld.climb = FLT_MAX;
	cmdBufOld.heading = INT16_MAX;
	cmdBufOld.throttle = UINT16_MAX;
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
		ESP_LOGD(pcTaskGetTaskName(0),"cmdBuf.command=%d screen=%d", cmdBuf.command, screen);
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
					strcpy((char *)ascii, "alt		   : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "climb	   : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "heading	   : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
					ypos = ypos + fontHeight;
					strcpy((char *)ascii, "throttle    : ");
					lcdDrawString(&dev, fx, xpos, ypos, ascii, CYAN);
				}
				drawGeneral = 1;
				ypos = (fontHeight*3)-1;
				if (cmdBufOld.airspeed != cmdBuf.airspeed) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%f", cmdBuf.airspeed);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				ypos = ypos + fontHeight;
				if (cmdBufOld.groundspeed != cmdBuf.groundspeed) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%f", cmdBuf.groundspeed);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				ypos = ypos + fontHeight;
				if (cmdBufOld.alt != cmdBuf.alt) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%f", cmdBuf.alt);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				ypos = ypos + fontHeight;
				if (cmdBufOld.climb != cmdBuf.climb) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%f", cmdBuf.climb);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				ypos = ypos + fontHeight;
				if (cmdBufOld.heading != cmdBuf.heading) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%d", cmdBuf.heading);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				ypos = ypos + fontHeight;
				if (cmdBufOld.throttle != cmdBuf.throttle) {
					lcdDrawFillRect(&dev, xGeneral, ypos-fontHeight, SCREEN_WIDTH-1, ypos, BLACK);
					sprintf((char *)ascii, "%d", cmdBuf.throttle);
					lcdDrawString(&dev, fx, xGeneral, ypos, ascii, CYAN);
				}
				memcpy(&cmdBufOld, &cmdBuf, sizeof(cmdBuf));

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
				ESP_LOGD(pcTaskGetTaskName(0),"airspeed=%d", airspeed);
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

