/**
 * @file main.c
 * @brief Exmaple application for logicrom display driver
 * @version 0.1
 * @date 2022-07-05
 * 
 * @copyright Copyright (c) 2022 Waybyte Solutions
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <lib.h>
#include <ril.h>
#include <os_api.h>

#include <hw/display.h>

#define ILI9340_CASET 0x2A
#define ILI9340_PASET 0x2B
#define ILI9340_RAMWR 0x2C
#define ILI9340_RAMRD 0x2E

static inline void ili9340_data_array(uint8_t *data, uint32_t len)
{
	for (int i = 0; i < len; i++)
		display_write_data(data[i]);
}

static void ili9340_init(void)
{
	static const int init_sequence[] = {
		-1,0xCB,0x39,0x2C,0x00,0x34,0x02,-1,0xCF,0x00,0xC1,0x30,
		-1,0xE8,0x85,0x00,0x78,-1,0xEA,0x00,0x00,-1,0xED,0x64,0x03,0x12,0x81,
		-1,0xF7,0x20,-1,0xC0,0x23,-1,0xC1,0x10,-1,0xC5,0x3e,0x28,-1,0xC7,0x86,
		-1,0x36,0xE8,-1,0x3A,0x55,-1,0xB1,0x00,0x18,-1,0xB6,0x08,0x82,0x27,
		-1,0xF2,0x00,-1,0x26,0x01,
		-1,0xE0,0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00,
		-1,0xE1,0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F,
		-1,0x11,-2,120,-1,0x29,-1,0x2c,-3 };
	int ret;
	int i, j;
	unsigned char buf[64];
	char msg[128];
	char str[16];

	i = 0;
	ret = 0;

	while (i < 512) {
		if (init_sequence[i] == -3) {
			/* done */
			debug(DBG_OFF, "Display panel init Done\n");
			break;
		}
		if (init_sequence[i] >= 0) {
			debug(DBG_OFF, "missing delimiter at position %d\n", i);
			ret = -EINVAL;
			break;
		}
		if (init_sequence[i + 1] < 0) {
			debug(DBG_OFF, "missing value after delimiter %d at position %d\n",
					init_sequence[i], i);
			ret = -EINVAL;
			break;
		}
		switch (init_sequence[i]) {
		case -1:
			i++;
			/* make debug message */
			strcpy(msg, "");
			j = i + 1;
			while (init_sequence[j] >= 0) {
				sprintf(str, "0x%02X ", init_sequence[j]);
				strcat(msg, str);
				j++;
			}
			debug(DBG_INFO, "init: write(0x%02X) %s\n", init_sequence[i], msg);

			/* Write */
			j = 0;
			while (init_sequence[i] >= 0) {
				if (j > 63) {
					debug(DBG_OFF, "%s: Maximum register values exceeded\n",
							__func__);
					ret = -EINVAL;
					break;
				}
				buf[j++] = init_sequence[i++];
			}

			if (ret)
				break;

			/* write data */
			display_write_cmd(buf[0]);
			ili9340_data_array(&buf[1], j - 1);
			break;
		case -2:
			i++;
			debug(DBG_INFO, "init: mdelay(%d)\n", init_sequence[i]);
			usleep(init_sequence[i++] * 1000);
			break;
		default:
			debug(DBG_OFF, "unknown delimiter %d at position %d\n",
					init_sequence[i], i);
			ret = -EINVAL;
		}
		if (ret)
			break;
	}
}

static void ili9340_blit_prepare(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	static uint8_t ili9340_fb[4];

	/* Set the rectangular area */
	display_write_cmd(ILI9340_CASET);
	ili9340_fb[0] = (x0 >> 8);
	ili9340_fb[1] = (0x00FF & x0);
	ili9340_fb[2] = (x1 >> 8);
	ili9340_fb[3] = (0x00FF & x1);
	ili9340_data_array(ili9340_fb, 4);

	display_write_cmd(ILI9340_PASET);
	ili9340_fb[0] = (y0 >> 8);
	ili9340_fb[1] = (0x00FF & y0);
	ili9340_fb[2] = (y1 >> 8);
	ili9340_fb[3] = (0x00FF & y1);
	ili9340_data_array(ili9340_fb, 4);

	display_write_cmd(ILI9340_RAMWR);
}

/**
 * Display Panel Information structure
 * 
 * Display panel used for testing is a 3.2inch 320x240 LCD Panel
 * with ILI9340 display controller
 * 
 * Ref:
 * https://www.waveshare.com/wiki/3.2inch_RPi_LCD_(B)
 * 
 */
const struct lcd_drv_t ili9340 = {
	.reset_delay_us = 20000,
	.init_delay_us = 20000,
	.io_level_mv = 3000,
	.width = 320,
	.height = 240,
	.bg_color = 0,
	.output_format = LCD_OUT_FMT_16BIT_RGB565,
	.in_format = LCD_IN_FMT_RGB565,
	.spi_linemode = LCD_SPI_4WIRE,
	.spi_freq = 90000000,
	.ops = {
		.init = ili9340_init,
		.prepare = ili9340_blit_prepare,
	}
};

/**
 * Application main entry point
 */
int main(int argc, char *argv[])
{
	struct layer_cfg_t lcfg;
	int ret;

	/*
	 * Initialize library and Setup STDIO
	 */
	logicrom_init("/dev/ttyUSB0", NULL);

	printf("System Ready\n");

	/* Init Logicrom display driver */
	ret = display_init(&ili9340);
	if (ret) {
		printf("Display driver init: %d\n", ret);
		os_task_delete(os_task_getid());
	} else {
		printf("Display initialized: %d\n", ret);
	}
	/* for random colors */
	srand(time(NULL));

	/* Layer 0 is initialized by default by display driver to LCD size */
	/* Setup layer 1, with 50% opacity, size 100px x 100px */
	lcfg.format = LCD_IN_FMT_RGB565;
	lcfg.alpha = 128;
	lcfg.area.x = 100;
	lcfg.area.y = 100;
	lcfg.area.w = 100;
	lcfg.area.h = 100;
	lcfg.key_color = 0;
	lcfg.key_mask = 0;
	lcfg.key_en = 0;
	ret = display_layer_setup(DISPLAY_LAYER1, &lcfg);
	if (ret)
		printf("Display Layer 1 error: %d\n", ret);
	else
		printf("Display_layer 1 setup done\n");

	/* Setup layer 1, with 50% opacity, size 100px x 100px */
	lcfg.format = LCD_IN_FMT_RGB565;
	lcfg.alpha = 128;
	lcfg.area.x = 150;
	lcfg.area.y = 50;
	lcfg.area.w = 100;
	lcfg.area.h = 100;
	lcfg.key_color = 0;
	lcfg.key_mask = 0;
	lcfg.key_en = 0;
	ret = display_layer_setup(DISPLAY_LAYER2, &lcfg);
	if (ret)
		printf("Display Layer 2 error: %d\n", ret);
	else
		printf("Display_layer 2 setup done\n");

	/* Enable Layer 0 */
	display_layer_enable(DISPLAY_LAYER0, 1);
	/* Enable Layer 1 */
	display_layer_enable(DISPLAY_LAYER1, 1);
	/* Enable Layer 2 */
	display_layer_enable(DISPLAY_LAYER2, 1);

	printf("System Initialization finished\n");

	while (1) {
		/* Fill random color on layer 0 */
		uint16_t *buf = display_layer_getbuffer(DISPLAY_LAYER0);
		uint16_t color = rand();
		for (int i = 50; i < 150; i++) {
			for(int j = 50; j < 150; j++)
				buf[(320 * i) + j] = color;
		}
		/* Fill random color on layer 1 */
		buf = display_layer_getbuffer(DISPLAY_LAYER1);
		color = rand();
		for (int i = 0; i < 100; i++) {
			for(int j = 0; j < 100; j++)
				buf[(100 * i) + j] = color;
		}
		/* Fill random color on layer 2 */
		buf = display_layer_getbuffer(DISPLAY_LAYER2);
		color = rand();
		for (int i = 0; i < 100; i++) {
			for(int j = 0; j < 100; j++)
				buf[(100 * i) + j] = color;
		}

		/* Update display area used by Layers */
		struct lcd_area_t area = {
			.x = 50,
			.y = 50,
			.w = 200,
			.h = 150
		};
		display_flush(&area, FALSE);
		display_wait_transfer();
		/* sleep for 1s */
		sleep(1);
	}
}

