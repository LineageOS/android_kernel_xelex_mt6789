/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.h: Main H File.
 */
#ifndef BBQX0KBD_MAIN_H_
#define BBQX0KBD_MAIN_H_

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>

#include <linux/proc_fs.h> 
#include <linux/seq_file.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_panel_ext.h"
#include "mtk_disp_notify.h"
#endif

#include "config.h"
#include "bbqX0kbd_i2cHelper.h"
#include "bbqX0kbd_registers.h"
#include "debug_levels.h"
#if (BBQX0KBD_TYPE == BBQ10KBD_PMOD)
#include "bbq10kbd_pmod_codes.h"
#endif
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
#include "bbq10kbd_featherwing_codes.h"
#endif
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
#include "bbq20kbd_pmod_codes.h"
#endif

#define COMPATIBLE_NAME			"wallComputer,bbqX0kbd"
#define PDRV_COMPATIBLE_NAME			"wallComputer,pdrv_bbqX0kbd"
#define DEVICE_NAME				"Q25_keyboard"
#define BBQX0KBD_BUS_TYPE		BUS_I2C
#define BBQX0KBD_VENDOR_ID		0xBB20
#define BBQX0KBD_PRODUCT_ID		0x0025
#define BBQX0KBD_VERSION_ID		0x0001


#define LEFT_CTRL_BIT			BIT(0)
#define LEFT_SHIFT_BIT			BIT(1)
#define LEFT_ALT_BIT			BIT(2)
#define LEFT_GUI_BIT			BIT(3)
#define RIGHT_CTRL_BIT			BIT(4)
#define RIGHT_SHIFT_BIT			BIT(5)
#define RIGHT_ALT_BIT			BIT(6)
#define RIGHT_GUI_BIT			BIT(7)

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
#define BBQX0KBD_DEFAULT_WORK_RATE	40
#define BBQX0KBD_MINIMUM_WORK_RATE	10
#define BBQX0KBD_MAXIMUM_WORK_RATE	1000
#endif

struct bbqX0kbd_data {
#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
	struct work_struct work_struct;
	uint8_t fifoCount;
	uint8_t fifoData[BBQX0KBD_FIFO_SIZE][2];
#else
	struct delayed_work delayed_work;
	struct workqueue_struct *workqueue_struct;
	uint8_t work_rate_ms;
#endif

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	uint8_t touchInt;
	int8_t rel_x;
	int8_t rel_y;
	int8_t relX_offset;
	int8_t relY_offset;
#endif
	uint8_t version_number;
	uint8_t modifier_keys_status;
	uint8_t keyboardBrightness;
	uint8_t lastKeyboardBrightness;
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	uint8_t screenBrightness;
	uint8_t lastScreenBrightness;
#endif
	unsigned short keycode[NUM_KEYCODES];
	unsigned short pressed_keycode[NUM_KEYCODES];
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	
	int irq_gpio;
	int rst_gpio;
	int power_vdd_gpio;
	int power_1v8_gpio;
	int power_2v8_gpio;
	int firmware_download_gpio;
	
	bool suspended;
	
	//turn off/on mouse or key
	uint8_t q20_spec_switch_key_mouse;
	//keyboard poweroff/poweron
	uint8_t q20_spec_power_flag;
};

struct bbqX0kbd_data *g_bbqX0kbd_data;

#define PROC_NAME	"firmware_upgrade"
static struct proc_dir_entry *fwu_proc_entry;
#define Q20_SWITCH_KEY_MOUSE_PROC_NAME	"q20_switch_key_mouse"
static struct proc_dir_entry * q20_switch_key_mouse_proc_entry;
#define Q20_SPEC_POWER_FLAG_PROC_NAME	"q20_spec_power_flag"
static struct proc_dir_entry * q20_spec_power_flag_proc_entry;

static const struct i2c_device_id bbqX0kbd_i2c_device_id[] = {
	{ DEVICE_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bbqX0kbd_i2c_device_id);

static const struct of_device_id bbqX0kbd_of_device_id[] = {
	{ .compatible = COMPATIBLE_NAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, bbqX0kbd_of_device_id);

static const struct of_device_id bbqX0kbd_pdrv_device_id[] = {
	{ .compatible = PDRV_COMPATIBLE_NAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, bbqX0kbd_pdrv_device_id);

#endif
