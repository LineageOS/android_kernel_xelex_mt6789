// SPDX-License-Identifier: GPL-2.0
/*
 * aw95016.c   aw95016 gpio and key
 *
 * Copyright (c) 2024 Shanghai Awinic Technology Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <linux/leds.h>
#include <linux/fb.h>
#include <stddef.h>
#include "aw95016.h"

#define HRTIMER_FRAME	20
#define AW95016_DRIVER_VERSION	"V0.1.0"

static KEY_STATE key_map[] = {
/*	name		code				val */
	{"Q",		KEY_Q,			0},
	{"A",	    KEY_A,			0},
	{"Z",	    KEY_Z,			0},
	{"Y",	    KEY_Y,			0},
	{"H",	    KEY_H,			0},
	{"ALT",		KEY_ALTERASE,	0},

	{"W",		KEY_W,			0},
	{"S",		KEY_S,			0},
	{"X",		KEY_X,			0},
	{"U",		KEY_U,			0},
	{"J",		KEY_J,			0},
	{"B",		KEY_B,	0},

	{"E",		KEY_E,			0},
	{"D",		KEY_D,			0},
	{"C",		KEY_C,			0},
	{"I",		KEY_I,			0},
	{"K",		KEY_K,	    	0},
	{"N",		KEY_N,			0},

	{"R",		KEY_R,			0},
	{"F",		KEY_F,			0},
	{"V",		KEY_V,			0},
	{"O",		KEY_O,			0},
	{"L",		KEY_L,			0},
	{"M",		KEY_M,			0},

	{"T",	    KEY_T,	        0},
	{"G",		KEY_G,			0},
	{"Space",	KEY_SPACE,	    0},
	{"P",		KEY_P,			0},
	{"Del",		KEY_BACKSPACE,  0},
	{"Enter",	KEY_ENTER,		0},
};

/*********************************************************
 *
 * awxxxx i2c write/read byte
 *
 ********************************************************/
static unsigned int
i2c_write_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char reg_data)
{
	int ret = 0;
	unsigned char wdbuf[2] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
	};

	wdbuf[0] = reg_addr;
	wdbuf[1] = reg_data;

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		pr_err("msg %s i2c write error: %d\n", __func__, ret);

	return ret;

}

static unsigned int
i2c_read_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *val)
{
	int ret = 0;
	unsigned char rdbuf[2] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg_addr,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= rdbuf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	*val = rdbuf[0];

	return ret;
}


/*********************************************************
 *
 * awxxxx i2c write bytes
 *
 ********************************************************/
static int i2c_write_multi_byte(struct i2c_client *client,
				unsigned char reg_addr,
				unsigned char *buf, unsigned int len)
{
	int ret = 0;
	unsigned char *wdbuf;

	wdbuf = kmalloc(len + 1, GFP_KERNEL);
	if (wdbuf == NULL)
		return -ENOMEM;

	wdbuf[0] = reg_addr;
	memcpy(&wdbuf[1], buf, len);

	ret = i2c_master_send(client, wdbuf, len + 1);
	if (ret < 0)
		pr_err("%s: i2c master send error\n", __func__);

	kfree(wdbuf);

	return ret;
}

static unsigned int i2c_read_multi_byte(struct i2c_client *client,
					unsigned char reg_addr,
					unsigned char *buf,
					unsigned int len)
{
	int ret = 0;
	unsigned char *rdbuf = NULL;

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg_addr,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
		},
	};

	rdbuf = kmalloc(len, GFP_KERNEL);
	if (rdbuf == NULL)
		return  -ENOMEM;

	msgs[1].buf = rdbuf;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	if (buf != NULL)
		memcpy(buf, rdbuf, len);
	kfree(rdbuf);
	return ret;
}

/* val 1:open drain    0-> push pull */
static void aw95016_set_port_output_mode(struct aw95016 *aw95016, unsigned char val)
{
	unsigned char reg_val[2] = {0};

	i2c_read_multi_byte(aw95016->client, P0_DOMD, reg_val, ARRAY_SIZE(reg_val));
	if (val) {
		reg_val[0] |= 0xFF; /*open drain mode*/
		reg_val[1] |= 0xFF; /*open drain mode*/
	} else {
		reg_val[0] &= 0x00;/*push pull mode*/
		reg_val[1] &= 0x00;/*push pull mode*/
	}
	i2c_write_multi_byte(aw95016->client, P0_DOMD, reg_val, ARRAY_SIZE(reg_val));
}

/* val 1-> output  0->input */
static void
aw95016_set_port_direction_by_mask(struct aw95016 *aw95016, unsigned int mask, int val)
{
	unsigned char reg_val[2] = {0};
	int i = 0;

	i2c_read_multi_byte(aw95016->client, P0_DIR, reg_val, ARRAY_SIZE(reg_val));
	/* 0-output 1-input */
	for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
		if (mask & (0x01 << i)) {
			if (val) {
				if (i < 8)
					reg_val[0] |= 0x1 << i;
				else
					reg_val[1] |= 0x1 << (i - 8);
			} else {
				if (i < 8)
					reg_val[0] &= ~(0x1 << i);
				else
					reg_val[1] &= ~(0x1 << (i - 8));
			}
		}
	}
	i2c_write_multi_byte(aw95016->client, P0_DIR, reg_val, ARRAY_SIZE(reg_val));
}


/* val 1-> output high  0->output low */
static void aw95016_set_port_output_state_by_mask(struct aw95016 *aw95016, unsigned int mask, int val)
{
	unsigned char reg_val[2] = {0};
	int i = 0;

	i2c_read_multi_byte(aw95016->client, P0_OUTPUT, reg_val, ARRAY_SIZE(reg_val));
	for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
		if (mask & (0x01 << i)) {
			if (val) {
				if (i < 8)
					reg_val[0] |= 0x1 << i;
				else
					reg_val[1] |= 0x1 << (i - 8);
			} else {
				if (i < 8)
					reg_val[0] &= ~(0x1 << i);
				else
					reg_val[1] &= ~(0x1 << (i - 8));
			}
		}
	}
	i2c_write_multi_byte(aw95016->client, P0_OUTPUT, reg_val, ARRAY_SIZE(reg_val));
}

/* val 1 -> enable 0 - > disable */
static void
aw95016_enbale_interrupt_by_mask(struct aw95016 *aw95016, unsigned int mask, unsigned int val)
{
	unsigned char reg_val[2] = {0};
	int i = 0;

	i2c_read_multi_byte(aw95016->client, P0_MAK, reg_val, ARRAY_SIZE(reg_val));
	/* 0 enable 1 disable */
	for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
		if (mask & (0x01<<i)) {
			if (!val) {
				if (i < 8)
					reg_val[0] |= 0x1 << i;
				else
					reg_val[1] |= 0x1 << (i - 8);
			} else {
				if (i < 8)
					reg_val[0] &= ~(0x1 << i);
				else
					reg_val[1] &= ~(0x1 << (i - 8));
			}
		}
	}
	i2c_write_multi_byte(aw95016->client, P0_MAK, reg_val, ARRAY_SIZE(reg_val));
}

/* Must be read single-byte */
static void aw95016_clear_interrupt(struct aw95016 *aw95016)
{
	unsigned char reg_val[2] = {0};

	i2c_read_byte(aw95016->client, P0_INPUT, &reg_val[0]);
	i2c_read_byte(aw95016->client, P1_INPUT, &reg_val[1]);
}

static int aw95016_reset(struct aw95016 *aw95016)
{
	int ret = 0;
	struct device_node *node = aw95016->dev->of_node;

	aw95016->rst_gpio = of_get_named_gpio(node, "reset-gpio", 0);

	if ((!gpio_is_valid(aw95016->rst_gpio))) {
		dev_err(aw95016->dev, "%s: dts don't provide reset-gpio\n", __func__);
		return -EINVAL;
	}

	ret = gpio_request(aw95016->rst_gpio, "aw95016-reset");
	if (ret) {
		dev_err(&aw95016->client->dev, "%s: unable to request gpio [%d]\n", __func__,
			aw95016->rst_gpio);
		return ret;
	}

	ret = gpio_direction_output(aw95016->rst_gpio, 1);
	if (ret) {
		gpio_free(aw95016->rst_gpio);
		dev_err(aw95016->dev, "%s: unable to set direction of gpio\n", __func__);
		return ret;
	}
	gpio_set_value(aw95016->rst_gpio, 1);
	usleep_range(1000, 2000);
	gpio_set_value(aw95016->rst_gpio, 0);
	usleep_range(1000, 2000);
	gpio_set_value(aw95016->rst_gpio, 1);
	usleep_range(6000, 6500);
	return ret;
}

static int aw95016_parse_dts(struct aw95016 *aw95016)
{
	int ret = 0;
	unsigned int val = 0;
	struct device_node *np = NULL;

	np = aw95016->dev->of_node;

	aw95016->single_key_enable = false;
	ret = of_property_read_u32(np, "aw95016,single_key_enable", &val);
	if (ret) {
		dev_err(aw95016->dev, "%s: single_key_enable undefined\n", __func__);
	} else {
		AW_DEBUG("%s:single_key_enable is defined\n", __func__);
		if (val == 1)
			aw95016->single_key_enable = true;
	}

	aw95016->matrix_key_enable = false;
	ret = of_property_read_u32(np, "aw95016,matrix_key_enable", &val);
	if (ret) {
		dev_err(aw95016->dev, "%s:key_enable undefined\n", __func__);
	} else {
		AW_DEBUG("%s:key_enable provided is defined\n", __func__);
		if (val == 1)
			aw95016->matrix_key_enable = true;
	}

	aw95016->gpio_feature_enable = false;
	ret = of_property_read_u32(np, "aw95016,gpio_enable", &val);
	if (ret < 0) {
		dev_err(aw95016->dev, "%s:gpio_enable undefined\n", __func__);
	} else {
		AW_DEBUG("%s:aw95016,gpio_enable is defined\n", __func__);
		if (val == 1)
			aw95016->gpio_feature_enable = true;
	}

	aw95016->kb_blk_gpio = of_get_named_gpio(np, "aw95016,kb-blk-gpio", 0);
	if (!gpio_is_valid(aw95016->kb_blk_gpio)) {
		dev_info(aw95016->dev, "%s: kb-blk-gpio not defined\n", __func__);
	}

	return ret;
}

/*********************************************************
 *
 * aw95016 reg
 *
 ********************************************************/
static ssize_t reg_show(struct device *cd, struct device_attribute *attr, char *buf)
{
	unsigned char reg_val = 0;
	unsigned char i = 0;
	ssize_t len = 0;
	struct aw95016 *aw95016 = dev_get_drvdata(cd);
	struct i2c_client *client = aw95016->client;

	for (i = 0; i <= 0x17; i++) {
		i2c_read_byte(client, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", i, reg_val);
	}
	i2c_read_byte(client, 0x1A, &reg_val);
	len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", 0x1A, reg_val);
	i2c_read_byte(client, 0x60, &reg_val);
	len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", 0x60, reg_val);
	i2c_read_byte(client, 0x61, &reg_val);
	len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", 0x61, reg_val);

	return len;
}

static ssize_t
reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[2];
	struct aw95016 *aw95016 = dev_get_drvdata(dev);
	struct i2c_client *client = aw95016->client;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		i2c_write_byte(client, databuf[0], databuf[1]);

	return len;
}

static DEVICE_ATTR_RW(reg);

static int aw95016_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	return err;
}

/*********************************************************
 *
 * aw95016 check chipid
 *
 ********************************************************/
static int aw95016_read_chipid(struct i2c_client *client)
{
	unsigned char val;
	unsigned char cnt;

	for (cnt = 5; cnt > 0; cnt--) {
		i2c_read_byte(client, RESET, &val);

		AW_DEBUG("%s val=0x%x\n", __func__, val);
		if (val == AW95016_ID)
			return 0;
		mdelay(5);
	}
	return -EINVAL;
}

/*********************************************************
 *
 * aw95016 key feature
 *
 ********************************************************/
static int aw95016_parse_dt_for_key(struct aw95016_key *p_key_data, struct device_node *np)
{
	int ret = 0;
	int i = 0;
	unsigned int val = 0;
	struct aw95016 *aw95016 = p_key_data->priv;

	p_key_data->wake_up_enable = false;
	ret = of_property_read_u32(np, "aw95016,wake_up_enable", &val);
	if (ret == 0) {
		if (val == 1)
			p_key_data->wake_up_enable = true;
	} else {
		dev_err(aw95016->dev, "%s: no aw95016,wake_up_enable, abort\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "aw95016,input_port_mask", &p_key_data->input_port_mask);
	if (ret) {
		dev_err(aw95016->dev, "%s: no aw95016,input_port_mask, abort\n", __func__);
		return ret;
	}

	p_key_data->input_port_nums = 0;
	for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
		if (p_key_data->input_port_mask & (0x01 << i))
			p_key_data->input_port_nums++;
	}

	if (aw95016->single_key_enable) {
		p_key_data->key_mask =  p_key_data->input_port_mask;
		AW_DEBUG("aw95016 key input_port_mask = 0x%x, input_num	= %d\n",
						p_key_data->input_port_mask,
						p_key_data->input_port_nums);
	}
	if (aw95016->matrix_key_enable) {
		ret = of_property_read_u32(np, "aw95016,output_port_mask",
					&p_key_data->output_port_mask);
		if (ret) {
			dev_err(aw95016->dev, "%s, no aw95016,output_port_mask\n", __func__);
			return ret;
		}
		p_key_data->output_port_nums = 0;
		for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
			if (p_key_data->output_port_mask & (0x01 << i))
				p_key_data->output_port_nums++;
		}
		p_key_data->key_mask
		= p_key_data->input_port_mask | p_key_data->output_port_mask;
		AW_DEBUG("aw95016 key output_port_mask = 0x%x, output_nmu = %d\n",
						p_key_data->output_port_mask,
						p_key_data->output_port_nums);
	}

	return 0;
}

irqreturn_t aw95016_irq_func(int irq, void *key_data)
{
	struct aw95016_key *p_key_data = (struct aw95016_key *)key_data;
	struct aw95016 *aw95016 = p_key_data->priv;

	disable_irq_nosync(p_key_data->priv->irq_num);
	/* disable aw95016 input interrupt */
	aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 0);
	aw95016_clear_interrupt(aw95016);
	schedule_delayed_work(&p_key_data->int_work, msecs_to_jiffies(p_key_data->debounce_delay));
	return 0;
}

static int aw95016_irq_register(struct aw95016 *aw95016)
{
	int ret = 0;
	struct device_node *np = aw95016->dev->of_node;

	aw95016->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw95016->irq_gpio < 0) {
		dev_err(aw95016->dev, "%s: get irq gpio failed\r\n", __func__);
		return -EINVAL;
	}

	//ret = devm_gpio_request(aw95016->dev, aw95016->irq_gpio, "aw95016 irq gpio");
	//if (ret) {
	//	dev_err(aw95016->dev, "%s: devm_gpio_request irq gpio failed\r\n", __func__);
	//	return -EBUSY;
	//}
	//gpio_direction_input(aw95016->irq_gpio);
	aw95016->irq_num = gpio_to_irq(aw95016->irq_gpio);
	if (aw95016->irq_num < 0) {
		ret = aw95016->irq_num;
		dev_err(aw95016->dev, "%s gpio to irq failed\r\n", __func__);
		goto err;
	}
	AW_DEBUG("aw95016 irq num=%d\n", aw95016->irq_num);
	ret = devm_request_threaded_irq(aw95016->dev, aw95016->irq_num, NULL,
					aw95016_irq_func,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"aw95016_irq", aw95016->key_data);
	if (ret < 0) {
		dev_err(aw95016->dev, "%s register irq failed\r\n", __func__);
		goto err;
	}
	/* enable_irq_wake(aw95016->irq_num); */
	device_init_wakeup(aw95016->dev, 1);
	return 0;
err:
	//devm_gpio_free(aw95016->dev, aw95016->irq_gpio);
	return ret;
}

static int aw95016_input_dev_register(struct aw95016 *aw95016)
{
	int ret = 0;
	int i = 0;
	struct aw95016_key *p_key_data = aw95016->key_data;

	p_key_data->input = input_allocate_device();

	if (p_key_data->input == NULL) {
		ret = -ENOMEM;
		dev_err(aw95016->dev, "%s: failed to allocate input device\n", __func__);
		return ret;
	}
	p_key_data->input->name = "P26-keyboard";
	p_key_data->input->dev.parent = aw95016->dev;
	p_key_data->keymap_len = sizeof(key_map) / sizeof(KEY_STATE);
	p_key_data->keymap = (KEY_STATE *)&key_map;
	input_set_drvdata(p_key_data->input, p_key_data);

	__set_bit(EV_KEY, p_key_data->input->evbit);
	__set_bit(EV_SYN, p_key_data->input->evbit);
	for (i = 0; i < p_key_data->keymap_len; i++)
		__set_bit(p_key_data->keymap[i].key_code & KEY_MAX, p_key_data->input->keybit);

	ret = input_register_device(p_key_data->input);
	if (ret) {
		input_free_device(p_key_data->input);
		dev_err(aw95016->dev, "%s: failed to allocate input device\n", __func__);
		return ret;
	}
	return 0;
}

void aw95016_int_work(struct work_struct *work)
{
	struct delayed_work *p_delayed_work = container_of(work, struct delayed_work, work);
	struct aw95016_key *p_key_data = container_of(p_delayed_work, struct aw95016_key, int_work);

	AW_DEBUG("%s enter, %d\n", __func__, __LINE__);
	schedule_work(&p_key_data->key_work);
}
/* val 1-> output high  0->output low */
static unsigned int aw95016_get_port_input_state(struct aw95016 *aw95016)
{
	unsigned char reg_val[2] = {0};

	i2c_read_byte(aw95016->client, P0_INPUT, &reg_val[0]);
	i2c_read_byte(aw95016->client, P1_INPUT, &reg_val[1]);
	return reg_val[0] | (reg_val[1] << 8);
}

static void aw95016_key_work(struct work_struct *work)
{
	int i = 0;
	int j = 0;
	int real_idx = 0;
	int real_row = 0;
	int real_col = 0;
	int key_code = 0;
	int key_val = 0;
	int key_num = 0;
	unsigned int retry_state;
	unsigned int input_val = 0;
	struct aw95016_key *p_key_data = container_of(work, struct aw95016_key, key_work);
	struct aw95016 *aw95016 = p_key_data->priv;
	unsigned int *new_state = p_key_data->new_output_state;
	unsigned int *old_state = p_key_data->old_output_state;
	unsigned char reg_val[2] = {0};

/*	if wake_up_enable set to 1,the key fuction can be use in suspend mode.*/
/*	if wake_up_enable set to 0,the key fuction will be banned in suspend mode.*/

	if ((!p_key_data->wake_up_enable) && (!aw95016->screen_state)) {
		enable_irq(aw95016->irq_num);
		aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 1);
		aw95016_clear_interrupt(aw95016);
		return;
	}

	if (aw95016->matrix_key_enable) {
		for (i = 0, real_idx = 0; i < AW95016_KEY_PORT_MAX; i++) {
			if (p_key_data->output_port_mask & (0x01 << i)) {
                i2c_read_multi_byte(aw95016->client, P0_OUTPUT, reg_val, ARRAY_SIZE(reg_val));
				input_val = reg_val[0] | (reg_val[1] << 8);
				input_val |= p_key_data->output_port_mask;
				input_val &= ~(0x01 << i);

				reg_val[0] = input_val & 0xff;
				reg_val[1] = (input_val >> 8) & 0xff;

				i2c_write_multi_byte(aw95016->client, P0_OUTPUT, reg_val, ARRAY_SIZE(reg_val));
				i2c_read_multi_byte(aw95016->client, P0_INPUT, reg_val, ARRAY_SIZE(reg_val));
				new_state[real_idx] = (reg_val[0] | (reg_val[1] << 8)) & p_key_data->input_port_mask;
				real_idx++;
			}
		}

		/* key state change */
		if (memcmp(&new_state[0], &old_state[0], p_key_data->output_port_nums * sizeof(unsigned int))) {
			for (i = 0; i <  p_key_data->output_port_nums; i++) {
				AW_DEBUG("aw95016 new_state[%d]: 0x%x, old_state[%d]:0x%x\n", i, new_state[i], i, old_state[i]);
			}

			/* stage changed */
			for (i = 0, real_col = 0; i < AW95016_KEY_PORT_MAX; i++) {
				if (p_key_data->output_port_mask & (0x01 << i)) {
					if (new_state[real_col] != old_state[real_col]) {
						for (j = 0, real_row = 0; j < AW95016_KEY_PORT_MAX; j++) {
							if (p_key_data->input_port_mask & (0x01 << j)) {
								if ((new_state[real_col] & (0x01 << j)) != (old_state[real_col] & (0x01 << j))) {
									key_code = p_key_data->keymap[real_row * p_key_data->output_port_nums + real_col].key_code;
									key_val = (old_state[real_col] & (0x01 << j)) ? 1 : 0;/* press or release */
									AW_DEBUG("aw95016 report: key_code = %d, key_val = %d\n", key_code, key_val);
									AW_DEBUG("aw95016 real_row = %d, real_col = %d\n", real_row, real_col);
									input_report_key(p_key_data->input, key_code, key_val);
									input_sync(p_key_data->input);
								}
								real_row++;
							}
						}
					}
					real_col++;
				}
			}
		}

		memcpy(&old_state[0], &new_state[0], p_key_data->output_port_nums * sizeof(unsigned int));

		/* all key release */
		if (!memcmp(&new_state[0], &p_key_data->def_output_state[0],
			p_key_data->output_port_nums * sizeof(unsigned int))) {
			aw95016_set_port_direction_by_mask(aw95016, p_key_data->output_port_mask, 1); /* set output mode */
			aw95016_set_port_output_state_by_mask(aw95016, p_key_data->output_port_mask, 0);/* set output low */
			aw95016_clear_interrupt(aw95016); /* clear inputerrupt */
			enable_irq(aw95016->irq_num);
			aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 1);/* enable input interrupt */
			retry_state = aw95016_get_port_input_state(aw95016);
			if ((retry_state & p_key_data->input_port_mask) != p_key_data->input_port_mask) {
				disable_irq_nosync(aw95016->irq_num);
				aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 0);
			} else {
				AW_DEBUG("%s enter, all key release.\n", __func__);
				return;
			}
		}
		hrtimer_start(&p_key_data->key_timer, ktime_set(0, (1000 / HRTIMER_FRAME) * 1000), HRTIMER_MODE_REL);
	}

	if (aw95016->single_key_enable) {
		p_key_data->new_input_state = aw95016_get_port_input_state(aw95016);
		p_key_data->new_input_state &= p_key_data->input_port_mask;
		real_idx = 0;
		if (p_key_data->new_input_state != p_key_data->old_input_state) {
			for (i = 0; i < AW95016_KEY_PORT_MAX; i++) {
				if (p_key_data->input_port_mask & (0x01 << i)) {
					if ((p_key_data->new_input_state & (0x01 << i)) != (p_key_data->old_input_state & (0x01 << i))) {
						key_code = p_key_data->keymap[real_idx].key_code;
						key_val = (p_key_data->old_input_state & 0x01 << i) ? 1 : 0; /* press or release */

						key_num = aw95016_separate_key_data[real_idx];
						/*if (key_val == 1) */
						/*	AW_DEBUG("%s, key%d pressed\n", __func__, key_num); */
						/*else */
						/*	AW_DEBUG("%s, key%d release\n", __func__, key_num); */
						/* AW_DEBUG("%s, key_code = 0x%x, key_val = 0x%x\r\n", __func__, key_code, key_val); */
						input_report_key(p_key_data->input, key_code, key_val);
						input_sync(p_key_data->input);
					}
					real_idx++;
				}
			}
		}

		p_key_data->old_input_state = p_key_data->new_input_state;
		aw95016_clear_interrupt(aw95016);
		enable_irq(aw95016->irq_num);
		/* enable input interrupt */
		aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 1);
		return;
	}
}

static enum hrtimer_restart aw95016_timer_func(struct hrtimer *p_hrtimer)
{
	struct aw95016_key *p_key_data = container_of(p_hrtimer, struct aw95016_key, key_timer);

	schedule_work(&p_key_data->key_work);
	return HRTIMER_NORESTART;
}

static void aw95016_kb_brightness_set(struct led_classdev *led_cdev,
				      enum led_brightness brightness)
{
	struct aw95016 *aw95016 = container_of(led_cdev, struct aw95016, kb_led_cdev);

	if (gpio_is_valid(aw95016->kb_blk_gpio))
		gpio_set_value(aw95016->kb_blk_gpio, brightness ? 1 : 0);
}

static void aw95016_key_chip_init(struct aw95016_key *p_key_data)
{
	unsigned int all_mask = 0;
	struct aw95016 *aw95016 = p_key_data->priv;

	disable_irq_nosync(p_key_data->priv->irq_num);
	aw95016_enbale_interrupt_by_mask(aw95016, AW95016_INT_MASK, 0); /* disale p0 p1 interrupt */
	all_mask = p_key_data->input_port_mask | p_key_data->output_port_mask;
	//aw95016_set_port_mode_by_mask(aw95016, all_mask, 1); /*AW95016 默认就是gpio模式 */
	aw95016_set_port_direction_by_mask(aw95016, p_key_data->input_port_mask, 0); /* input mode */
	aw95016_set_port_direction_by_mask(aw95016, p_key_data->output_port_mask, 1); /* output mode */
	aw95016_set_port_output_mode(aw95016, 1); /* set output port pull push mode */

	aw95016_set_port_output_state_by_mask(aw95016, p_key_data->output_port_mask, 0);/* set output low */

	aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 1); /* enable input interrupt */
	/* clear inputerrupt */
	aw95016_clear_interrupt(aw95016);
	enable_irq(p_key_data->priv->irq_num);
}

static void aw95016_key_free_all_resource(struct aw95016 *aw95016)
{
	if (aw95016->matrix_key_enable) {
		devm_kfree(aw95016->dev, aw95016->key_data->new_output_state);
		devm_kfree(aw95016->dev, aw95016->key_data->old_output_state);
		devm_kfree(aw95016->dev, aw95016->key_data->def_output_state);
		input_unregister_device(aw95016->key_data->input);
		input_free_device(aw95016->key_data->input);
		devm_gpio_free(aw95016->dev, aw95016->irq_gpio);
		devm_kfree(aw95016->dev, aw95016->key_data);
	}
}

static void aw95016_single_key_chip_init(struct aw95016_key *p_key_data)
{
	unsigned int all_mask = 0;
	struct aw95016 *aw95016 = p_key_data->priv;

	disable_irq_nosync(p_key_data->priv->irq_num);
	aw95016_enbale_interrupt_by_mask(aw95016, AW95016_INT_MASK, 0); /* disale p0 p1 interrupt */
	all_mask = p_key_data->input_port_mask;
	p_key_data->old_input_state = p_key_data->input_port_mask;
	//aw95016_set_port_mode_by_mask(aw95016, all_mask, 1); /*AW95016 默认就是gpio模式 */
	aw95016_set_port_direction_by_mask(aw95016, p_key_data->input_port_mask, 0); /* input mode */
	aw95016_set_port_output_mode(aw95016, 1); /* set output port pull push mode */

	aw95016_enbale_interrupt_by_mask(aw95016, p_key_data->input_port_mask, 1);/* enable input interrupt */
	/* clear inputerrupt */
	aw95016_clear_interrupt(aw95016);
	enable_irq(p_key_data->priv->irq_num);
}

static int aw95016_key_feature_init(struct aw95016 *aw95016)
{
	int ret = 0;
	int i = 0;
	struct aw95016_key *p_key_data = NULL;
	struct device_node *key_node = NULL;

	p_key_data = devm_kzalloc(aw95016->dev, sizeof(struct aw95016_key), GFP_KERNEL);
	if (p_key_data == NULL)
		return -ENOMEM;

	aw95016->key_data = p_key_data;
	p_key_data->priv = aw95016;
	key_node = of_find_node_by_name(aw95016->dev->of_node, "aw95016,key");
	if (key_node == NULL) {
		dev_err(aw95016->dev, "%s: can't find aw95016,key node return failed\n", __func__);
		ret = -EINVAL;
		goto err_id;
	}
	ret = aw95016_parse_dt_for_key(p_key_data, key_node);
	if (ret) {
		dev_err(aw95016->dev, "aw95016_parse_dt_for_key failed, check dts\n");
		goto err_id;
	}
	p_key_data->old_output_state =
				devm_kzalloc(aw95016->dev,
					     sizeof(unsigned int) * p_key_data->output_port_nums,
					     GFP_KERNEL);
	if (p_key_data->old_output_state == NULL) {
		dev_err(aw95016->dev, "%s:p_key_data->old_output_state malloc memory failed\r\n", __func__);
		goto err_id;
	}
	p_key_data->new_output_state = devm_kzalloc(aw95016->dev,
						    sizeof(unsigned int) * p_key_data->output_port_nums,
						    GFP_KERNEL);
	if (p_key_data->new_output_state == NULL) {
		dev_err(aw95016->dev, "%s:p_key_data->new_output_state malloc memory failed\r\n", __func__);
		goto free_old_output;
	}

	p_key_data->def_output_state = devm_kzalloc(aw95016->dev,
						    sizeof(unsigned int) * p_key_data->output_port_nums, GFP_KERNEL);
	if (p_key_data->def_output_state  == NULL) {
		ret = -ENOMEM;
		goto free_new_output;
	}

	for (i = 0; i < p_key_data->output_port_nums; i++) {
		p_key_data->new_output_state[i] = p_key_data->input_port_mask;
		p_key_data->old_output_state[i] = p_key_data->input_port_mask;
		p_key_data->def_output_state[i] = p_key_data->input_port_mask;
	}

	ret = aw95016_input_dev_register(aw95016);
	if (ret < 0) {
		dev_err(aw95016->dev, "%s input dev register failed\r\n", __func__);
		goto free_def_output;
	}

	p_key_data->debounce_delay = 1;
	INIT_DELAYED_WORK(&p_key_data->int_work, aw95016_int_work);
	INIT_WORK(&p_key_data->key_work, aw95016_key_work);
	hrtimer_init(&p_key_data->key_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	p_key_data->key_timer.function = aw95016_timer_func;
	ret = aw95016_irq_register(aw95016);
	if (ret < 0) {
		dev_err(aw95016->dev, "%s irq register failed\r\n", __func__);
		goto input_dev;
	}

	if (aw95016->matrix_key_enable)
		aw95016_key_chip_init(p_key_data);

	if (aw95016->single_key_enable)
		aw95016_single_key_chip_init(p_key_data);

	if (gpio_is_valid(aw95016->kb_blk_gpio)) {
		int led_ret;
		aw95016->kb_led_cdev.name = "i2c::kbd_backlight";
		aw95016->kb_led_cdev.default_trigger = "kbd-backlight";
		aw95016->kb_led_cdev.brightness_set = aw95016_kb_brightness_set;
		aw95016->kb_led_cdev.max_brightness = 255;
		led_ret = led_classdev_register(aw95016->dev, &aw95016->kb_led_cdev);
		if (led_ret)
			dev_err(aw95016->dev, "register kbd_backlight led failed: %d\n", led_ret);
	}

	return 0;
input_dev:
	input_unregister_device(p_key_data->input);
free_def_output:
	devm_kfree(aw95016->dev, p_key_data->def_output_state);
free_new_output:
	devm_kfree(aw95016->dev, p_key_data->new_output_state);
free_old_output:
	devm_kfree(aw95016->dev, p_key_data->old_output_state);
err_id:
	devm_kfree(aw95016->dev, aw95016->key_data);
	return ret;
}

/******************************************************
 *
 * gpio feature start
 *
 ******************************************************/
 /* gpio sys node */
/* echo gpio_idx dirction state > awgpio */
static ssize_t
awgpio_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int databuf[3];
	int i = 0;
	struct aw95016 *aw95016 = dev_get_drvdata(dev);
	struct aw95016_gpio *p_gpio_data = aw95016->gpio_data;
	struct aw95016_singel_gpio *p_single_gpio_data = p_gpio_data->single_gpio_data;

	if (sscanf(buf, "%x %x %x", &databuf[0], &databuf[1], &databuf[2]) == 3)
		AW_DEBUG("aw95016 gpio cmd param: %x %x %x\n", databuf[0], databuf[1], databuf[2]);

	if (p_gpio_data->gpio_mask & (0x01 << databuf[0])) {
		for (i = 0; i < p_gpio_data->gpio_num; i++) {
			if (p_single_gpio_data[i].gpio_idx == databuf[0]) {
				if (p_single_gpio_data[i].gpio_direction != databuf[1]) {
					p_single_gpio_data[i].gpio_direction = databuf[1];
					aw95016_set_port_direction_by_mask(aw95016, 0x1 << p_single_gpio_data[i].gpio_idx,
								  p_single_gpio_data[i].gpio_direction);
				}
				if (p_single_gpio_data[i].gpio_direction == 0x01) { /* output */
					if (p_single_gpio_data[i].state != databuf[2]) {
						p_single_gpio_data[i].state = databuf[2];
						aw95016_set_port_output_state_by_mask(aw95016,
										 0x1 << p_single_gpio_data[i].gpio_idx,
										 p_single_gpio_data[i].state);
					}
				}
			}
		}
	}
	return len;
}

static ssize_t awgpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i = 0;
	struct aw95016 *aw95016 = dev_get_drvdata(dev);
	struct aw95016_singel_gpio *p_single_gpio_data = aw95016->gpio_data->single_gpio_data;

	len += snprintf(buf+len, PAGE_SIZE-len, "Uasge:echo gpio_idx dirction state > awgpio\n");
	for (i = 0; i < aw95016->gpio_data->gpio_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "aw95016 gpio idx = %x, dir = %x, state = %x\n",
				p_single_gpio_data[i].gpio_idx,
				p_single_gpio_data[i].gpio_direction,
				p_single_gpio_data[i].state);
	}
	return len;
}


static DEVICE_ATTR_RW(awgpio);
static struct attribute *aw95016_gpio_attributes[] = {
	 &dev_attr_awgpio.attr,
	 NULL
};
static struct attribute_group aw95016_gpio_attribute_group = {
	.attrs = aw95016_gpio_attributes
};

static int aw95016_parse_for_single_gpio(struct device_node *gpio_node,
					 struct aw95016 *aw95016,
					 struct aw95016_gpio *p_gpio_data)
{
	int ret = 0;
	struct device_node *temp = NULL;
	int i = 0;
	struct aw95016_singel_gpio *p_single_gpio_data = p_gpio_data->single_gpio_data;

	for_each_child_of_node(gpio_node, temp) {
		ret = of_property_read_u32(temp, "aw95016,gpio_idx", &p_single_gpio_data[i].gpio_idx);
		if (ret < 0) {
			dev_err(aw95016->dev, "%s: no aw95016,gpio_idx, abort\n", __func__);
			goto err_id;
		}
		ret = of_property_read_u32(temp, "aw95016,gpio_dir", &p_single_gpio_data[i].gpio_direction);
		if (ret < 0) {
			dev_err(aw95016->dev, "%s, no aw95016,gpio_dir\n", __func__);
			goto err_id;
		}
		ret = of_property_read_u32(temp, "aw95016,gpio_default_val", &p_single_gpio_data[i].state);
		if (ret < 0) {
			dev_err(aw95016->dev, "%s, no aw95016,gpio_default_val, abort\n", __func__);
			goto err_id;
		}
		p_gpio_data->gpio_mask |= 0x01 << p_single_gpio_data[i].gpio_idx;
		AW_DEBUG("idx = %d, aw95016,gpio_idx = %d\n", i, p_single_gpio_data[i].gpio_idx);
		i++;
	}
	AW_DEBUG("%s gpio_mask = 0x%x\r\n", __func__, p_gpio_data->gpio_mask);
	return 0;
err_id:
	return ret;
}

static void aw95016_gpio_chip_init(struct aw95016 *aw95016, struct aw95016_gpio *p_gpio_data)
{
	int i = 0;
	struct aw95016_singel_gpio *p_single_gpio_data = p_gpio_data->single_gpio_data;

	/*AW95016 default is gpio mode */
	//aw95016_set_port_mode_by_mask(aw95016, p_gpio_data->gpio_mask, 1);
	aw95016_set_port_output_mode(aw95016, p_gpio_data->output_mode); /* OD or pull push mode */
	for (i = 0; i < p_gpio_data->gpio_num; i++) {
		aw95016_set_port_direction_by_mask(aw95016, 0x1 << p_single_gpio_data[i].gpio_idx, p_single_gpio_data[i].gpio_direction);
		if (p_single_gpio_data[i].gpio_direction) { /* output */
			aw95016_set_port_output_state_by_mask(aw95016, 0x1<<p_single_gpio_data[i].gpio_idx, p_single_gpio_data[i].state);
		}
	}
}

static void aw95016_gpio_free_all_resource(struct aw95016 *aw95016)
{
	if (aw95016->gpio_feature_enable) {
		devm_kfree(aw95016->dev, aw95016->gpio_data->single_gpio_data);
		devm_kfree(aw95016->dev, aw95016->gpio_data);
	}
}

static int aw95016_gpio_feature_init(struct aw95016 *aw95016)
{
	int ret = 0;
	int i = 0;
	struct device_node *gpio_node = NULL;
	struct aw95016_gpio *p_gpio_data = NULL;
	int gpio_num = 0;

	p_gpio_data = devm_kzalloc(aw95016->dev, sizeof(struct aw95016_gpio), GFP_KERNEL);
	if (p_gpio_data == NULL)
		return -ENOMEM;

	aw95016->gpio_data = p_gpio_data;

	gpio_node = of_find_node_by_name(aw95016->dev->of_node, "aw95016,gpio");
	if (gpio_node == NULL) {
		dev_err(aw95016->dev, "%s: can't find aw95016,gpio return failed\r\n", __func__);
		ret = -1;
		goto err_id;
	}
	ret = of_property_read_u32(gpio_node, "aw95016,gpio_mode", &p_gpio_data->output_mode);
	if (ret < 0) {
		dev_err(aw95016->dev, "%s: no aw95016,gpio_mode, abort\n", __func__);
		goto err_id;
	}

	gpio_num = of_get_child_count(gpio_node);
	p_gpio_data->gpio_num = gpio_num;
	p_gpio_data->single_gpio_data = devm_kzalloc(aw95016->dev,
					sizeof(struct aw95016_singel_gpio) * gpio_num,
					GFP_KERNEL);
	if (p_gpio_data->single_gpio_data == NULL) {
		dev_err(aw95016->dev, "%s: malloc memory failed\r\n", __func__);
		ret = -ENOMEM;
		goto err_id;
	}
	ret = aw95016_parse_for_single_gpio(gpio_node, aw95016, p_gpio_data);
	if (ret) {
		dev_err(aw95016->dev, "aw95016_parse_single_led failed\r\n");
		goto free_mem;
	}
	for (i = 0; i < gpio_num; i++)
		p_gpio_data->single_gpio_data[i].priv = aw95016;
	ret = sysfs_create_group(&aw95016->dev->kobj, &aw95016_gpio_attribute_group);
	if (ret) {
		dev_err(aw95016->dev, "led sysfs failed ret: %d\n", ret);
		goto free_mem;
	}
	aw95016_gpio_chip_init(aw95016, p_gpio_data);
	return 0;
free_mem:
	devm_kfree(aw95016->dev, p_gpio_data->single_gpio_data);
	devm_kfree(aw95016->dev, aw95016->gpio_data);
err_id:
	return ret;
}

static void aw95016_set_gpio_low_output(struct aw95016 *aw95016)
{
	i2c_write_byte(aw95016->client, P0_OUTPUT, 0);
	i2c_write_byte(aw95016->client, P1_OUTPUT, 0);
}

/*********************************************************
 *
 * aw95016 driver
 *
 ********************************************************/
static int aw95016_key_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct aw95016 *aw95016 = NULL;
	int ret = 0;

	AW_DEBUG("%s enter, %s\n", __func__, AW95016_DRIVER_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C transfer not Supported\n");
		return -EIO;
	}

	aw95016 = devm_kzalloc(&client->dev, sizeof(struct aw95016), GFP_KERNEL);
	if (!aw95016)
		return -ENOMEM;

	aw95016->dev = &client->dev;
	aw95016->client = client;
	i2c_set_clientdata(client, aw95016);

	ret = aw95016_reset(aw95016);
	if (ret)
		goto err_free_mem;
	if (aw95016_read_chipid(client)) {
		pr_err("read_chipid error\n");
		goto err_free_rst;
	}

	aw95016_set_gpio_low_output(aw95016);

	/* create debug dev node - reg */
	aw95016_create_sysfs(client);

	aw95016_parse_dts(aw95016);

	if (gpio_is_valid(aw95016->kb_blk_gpio)) {
		ret = devm_gpio_request_one(aw95016->dev, aw95016->kb_blk_gpio,
					    GPIOF_OUT_INIT_LOW, "aw95016-kb-blk");
		if (ret)
			dev_err(aw95016->dev, "%s: request kb-blk-gpio failed: %d\n", __func__, ret);
	}

	if (aw95016->single_key_enable) {
		/* single key init */
		ret = aw95016_key_feature_init(aw95016);
		if (ret) {
			dev_err(aw95016->dev, "aw95016 single key feature init failed \r\n");
			goto err_free_rst;
		}
	}

	if (aw95016->matrix_key_enable) {
		/* key init */
		ret = aw95016_key_feature_init(aw95016);
		if (ret) {
			dev_err(aw95016->dev, "aw95016 key feature init failed \r\n");
			goto err_free_rst;
		}
	}

	if (aw95016->gpio_feature_enable) {
		/* gpio init */
		ret = aw95016_gpio_feature_init(aw95016);
		if (ret) {
			dev_err(aw95016->dev, "aw95016 gpio feature init failed \r\n");
			goto free_key;
		}
	}
	aw95016->screen_state = true;

	AW_DEBUG("%s success!\n", __func__);
	return 0;
free_key:
	aw95016_key_free_all_resource(aw95016);
err_free_rst:
	gpio_free(aw95016->rst_gpio);
err_free_mem:
	devm_kfree(&client->dev, aw95016);
	AW_DEBUG("%s fail!\n", __func__);
	return ret;
}

static int aw95016_key_i2c_remove(struct i2c_client *client)
{
	struct aw95016 *aw95016 = i2c_get_clientdata(client);

	if (gpio_is_valid(aw95016->kb_blk_gpio))
		led_classdev_unregister(&aw95016->kb_led_cdev);
	aw95016_key_free_all_resource(aw95016);
	aw95016_gpio_free_all_resource(aw95016);
	devm_kfree(aw95016->dev, aw95016);
	return 0;
}

static int aw95016_suspend(struct device *dev)
{
	struct aw95016 *aw95016 = dev_get_drvdata(dev);
	struct aw95016_key *p_key_data = aw95016->key_data;

	aw95016->screen_state = false;

	if (!p_key_data) {
		AW_DEBUG("%s p_key_data is null\n", __func__);
		return -ENOMEM;
	}
	//aw95016_key_chip_init(p_key_data);
	
	if (!p_key_data->wake_up_enable) {
		if (gpio_is_valid(aw95016->rst_gpio)) {
			gpio_set_value(aw95016->rst_gpio, 0);
		}
	}

	if (gpio_is_valid(aw95016->kb_blk_gpio))
		gpio_set_value(aw95016->kb_blk_gpio, 0);

	return 0;
}

static int aw95016_resume(struct device *dev)
{
	struct aw95016 *aw95016 = dev_get_drvdata(dev);
	struct aw95016_key *p_key_data = aw95016->key_data;
	int i = 0;

	aw95016->screen_state = true;

	if (!p_key_data) {
		AW_DEBUG("%s p_key_data is null\n", __func__);
		return -ENOMEM;
	}
	for (i = 0; i < p_key_data->output_port_nums; i++) {
		p_key_data->new_output_state[i] = p_key_data->input_port_mask;
		p_key_data->old_output_state[i] = p_key_data->input_port_mask;
		p_key_data->def_output_state[i] = p_key_data->input_port_mask;
	}
	if (!p_key_data->wake_up_enable) {
		if (gpio_is_valid(aw95016->rst_gpio)) {
			gpio_set_value(aw95016->rst_gpio, 1);
			usleep_range(1000, 2000);
			gpio_set_value(aw95016->rst_gpio, 0);
			usleep_range(1000, 2000);
			gpio_set_value(aw95016->rst_gpio, 1);
			usleep_range(6000, 6500);
		}
	}

	aw95016_key_chip_init(p_key_data);

	return 0;
}

static const struct dev_pm_ops aw95016_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(aw95016_suspend, aw95016_resume)
};

static const struct of_device_id aw95016_keypad_of_match[] = {
	{ .compatible = "awinic,aw95016",},
	{},
};

static const struct i2c_device_id aw95016_key_i2c_id[] = {
	{"awinic,aw95016", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, aw95016_key_i2c_id);

static struct i2c_driver aw95016_key_i2c_driver = {
	.driver = {
		.name = "aw95016-key",
		.owner = THIS_MODULE,
		.of_match_table = aw95016_keypad_of_match,
		.pm = &aw95016_pm_ops,
	},
	.probe = aw95016_key_i2c_probe,
	.remove = aw95016_key_i2c_remove,
	.id_table = aw95016_key_i2c_id,
};

static int __init aw95016_key_i2c_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&aw95016_key_i2c_driver);
	if (ret) {
		pr_err("fail to add aw95016 device into i2c\n");
		return ret;
	}

	return 0;
}
module_init(aw95016_key_i2c_init);

static void __exit aw95016_key_i2c_exit(void)
{
	i2c_del_driver(&aw95016_key_i2c_driver);
}
module_exit(aw95016_key_i2c_exit);

MODULE_DESCRIPTION("aw95016 driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("<wangshuaijie@awinic.com>");
