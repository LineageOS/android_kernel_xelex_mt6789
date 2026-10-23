/*
 * Omnivision TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Omnivision Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND Omnivision
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL Omnivision BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF Omnivision WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, Omnivision'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/regulator/consumer.h>
#include <linux/fb.h>
#include "../tpd.h"
#include "s2716b_ts.h"
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include "hodafone_gesture.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/gpio/consumer.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>

extern struct tpd_device *tpd;

//allwinner
char twi_id = 0x01;
static int screen_max_x;
static int screen_max_y;
static int revert_x_flag;
static int revert_y_flag;
static int exchange_x_y_flag;
#define CTP_NAME        "s2716b_ts"
//end

// 定义触摸设备结构体
struct touch_device {
    struct i2c_client *client;
    int tp_flag;
    struct input_dev *input_dev;
    
    struct work_struct work;
	struct workqueue_struct *wq;
    
    int irq_gpio;
	int irq_on_state;
    int power_gpio;
	int power_on_state;
	int reset_gpio;
	int reset_on_state;
	int tpio_reset_gpio;
	unsigned int power_delay_ms;
	unsigned int reset_delay_ms;
	unsigned int reset_active_ms;
	unsigned long irq_flags;
	
    int irq;
};

// 全局触摸设备指针
struct touch_device *touch_dev;


static int gsl_ts_write(struct i2c_client *client, u8 addr, u8 *pdata, int datalen)
{
        int ret = 0;
        u8 tmp_buf[128];
        unsigned int bytelen = 0;

        if (datalen > 125) {
            pr_err("%s too big datalen = %d!\n", __func__, datalen);
            return -1;
        }

        tmp_buf[0] = addr;
        bytelen++;

        if (datalen != 0 && pdata != NULL) {
                memcpy(&tmp_buf[bytelen], pdata, datalen);
                bytelen += datalen;
        }

        ret = i2c_master_send(client, tmp_buf, bytelen);

        return ret;
}

static int gsl_ts_read(struct i2c_client *client, u8 addr, u8 *pdata, unsigned int datalen)
{
        int ret = 0;

        if (datalen > 126) {
                pr_err("%s too big datalen = %d!\n", __func__, datalen);
                return -1;
        }

        ret = gsl_ts_write(client, addr, NULL, 0);
        if (ret < 0) {
                pr_err("%s set data address fail!\n", __func__);
                return ret;
        }

        return i2c_master_recv(client, pdata, datalen);
}

// 初始化输入设备
static int touch_input_init(struct touch_device *dev) {

    s8 phys[32];
    dev->input_dev = input_allocate_device();//tpd->dev;
    if (!dev->input_dev) {
        S2716B_ERR("Failed to allocate input device\n");
        return -ENOMEM;
    }

    sprintf(phys, "input/ts");
    dev->input_dev->phys = phys;
    dev->input_dev->name = "s2716b_ts";
    dev->input_dev->id.bustype = BUS_I2C;
    dev->input_dev->id.vendor = 0xDEAD;
    dev->input_dev->id.product = 0xBEEF;
    dev->input_dev->id.version = 10427;
    input_set_drvdata(dev->input_dev, dev);

    // 设置输入设备的事件类型和按键码
    set_bit(EV_KEY, dev->input_dev->evbit);
    set_bit(EV_ABS, dev->input_dev->evbit);
    set_bit(BTN_TOUCH, dev->input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, dev->input_dev->propbit);
    // 设置坐标范围，根据实际屏幕分辨率调整
    input_set_abs_params(dev->input_dev, ABS_MT_TRACKING_ID, 0,255, 0, 0);
    input_set_abs_params(dev->input_dev, ABS_MT_POSITION_X, 0, 720, 0, 0);
    input_set_abs_params(dev->input_dev, ABS_MT_POSITION_Y, 0, 720, 0, 0);
    input_set_abs_params(dev->input_dev, ABS_MT_TOUCH_MAJOR, 0,255, 0, 0);
    input_set_abs_params(dev->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	// 注册输入设备
    if (input_register_device(dev->input_dev)) {
        S2716B_ERR("Failed to register input device\n");
        input_free_device(dev->input_dev);
        return -1;
    }
    
    return 0;
}

// 触摸数据读取函数
static void touch_read_data(struct work_struct *work) {
    u8 ts_data[9] = {0};
    u8 ts_data1[2] = {0};
    int raw_value;
    int x,y,w;
    gsl_ts_read(touch_dev->client,0x05,ts_data,9);
    //S2716B_LOG("---->%s: %x  %x %x  %x %x  %x %x  %x %x\n",__func__,ts_data[8],ts_data[7],ts_data[6],ts_data[5],ts_data[4],ts_data[3],ts_data[2],ts_data[1],ts_data[0]);
    gsl_ts_read(touch_dev->client,0x08,ts_data1,2);
    //S2716B_LOG("---->%s: %x  %x\n",__func__,ts_data1[1],ts_data1[0]);

    if (ts_data[0] == 0x04) {
		if (exchange_x_y_flag) {
			raw_value = (((ts_data[3] & 0x0F) << 8) | ts_data[2]);
			x = raw_value*screen_max_x/2500;

			raw_value = (((ts_data[5] & 0x0F) << 8) | ts_data[4]);
			y = raw_value*screen_max_y/2450;
			
		} else {
			raw_value = (((ts_data[3] & 0x0F) << 8) | ts_data[2]);
			y = raw_value*screen_max_y/2500;

			raw_value = (((ts_data[5] & 0x0F) << 8) | ts_data[4]);
			x = raw_value*screen_max_x/2450;
		}
        
        if (revert_x_flag) {
			x = screen_max_x - x;
		}
 
		if (revert_y_flag) {
			y = screen_max_y -y;
		}
		
		w = ts_data[6];

        if ((ts_data1[0] == 1) && (x < screen_max_x) && (y < screen_max_y) && (x > 0) && (y > 0)) {
            // 上报触摸按下事件
	    //S2716B_LOG("--report data----->x = %d,y = %d;w = %d\n", x, y , w);
	    input_report_key(touch_dev->input_dev, BTN_TOUCH, 1);
	    input_report_abs(touch_dev->input_dev, ABS_MT_TRACKING_ID, 1);
	    input_report_abs(touch_dev->input_dev, ABS_MT_TOUCH_MAJOR, w);
	    input_report_abs(touch_dev->input_dev, ABS_MT_POSITION_X, x);
	    input_report_abs(touch_dev->input_dev, ABS_MT_POSITION_Y, y);
	    input_report_abs(touch_dev->input_dev, ABS_MT_WIDTH_MAJOR, w);
            //input_mt_sync(touch_dev->input_dev);
	    input_sync(touch_dev->input_dev);//释放手指
        } else {
            // 上报触摸释放事件
	    //S2716B_LOG("touch release!\n");
            input_report_key(touch_dev->input_dev, BTN_TOUCH, 0);
            //input_mt_sync(touch_dev->input_dev);
	    input_sync(touch_dev->input_dev);//释放手指
        }
    }
    enable_irq(touch_dev->irq);
}

// 中断处理函数
static irqreturn_t touch_irq_handler(int irq, void *dev_id) {
    disable_irq_nosync(irq);
    queue_work(touch_dev->wq, &touch_dev->work);
    return IRQ_HANDLED;
}

// 触摸设备初始化函数
static int touch_init(struct i2c_client *client) {
    u8 mem_buf[25] = {0};
    u8 mem_buf00[1] = {0};
    u8 init_data[] =  {0};
    u8 init_data1[9] = {0x01,0x01,0x01,0x04,0x04,0x0a,0xc0,0x05,0x01};
    u8 init_data2[2] = {0x01,0x00};
    u8 init_data4[4] = {0xBB,0x09,0xBC,0x09};
    u8 init_data5[20] = {0x00,0x00,0x00,0x00,0x4C,0x04,0x6C,0x07,0x3C,0x32,0x9F,0x01,0x0C,0x05,0x1D,0x08,0xBD,0x09,0x0A,0x02};
    u8 init_data6[3] = {0x01,0x01,0x00};

#if 1
    gsl_ts_read(client,0x00,mem_buf00,1);
    S2716B_LOG("read 0x00  = %x %x \n",mem_buf[1], mem_buf[0]);
 
    gsl_ts_read(client,0xe9,mem_buf,6);
    S2716B_LOG("read 0xe9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);
    
    gsl_ts_read(client,0xe3,mem_buf,6);
    S2716B_LOG("read 0xe3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xdd,mem_buf,6);
    S2716B_LOG("read 0xdd  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xd7,mem_buf,6);
    S2716B_LOG("read 0xd7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x2E,mem_buf,10);
    S2716B_LOG("read 0x2E  = %x %x %x %x %x %x %x %x %x %x\n",mem_buf[9],mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x44,mem_buf,1);
    S2716B_LOG("read 0x44  = %x\n", mem_buf[0]);

    gsl_ts_read(client,0x45,mem_buf,5);
    S2716B_LOG("read 0x45  = %x %x %x %x %x\n",mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x47,mem_buf,1);
    S2716B_LOG("read 0x47  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x48,mem_buf,3);
    S2716B_LOG("read 0x48  = %x %x %x\n",mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xDD,mem_buf,6);
    S2716B_LOG("read 0xDD  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x04,mem_buf,1);
    S2716B_LOG("read 0x04  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x2E,mem_buf,10);
    S2716B_LOG("read 0x2E  = %x %x %x %x  %x %x %x %x %x %x\n",mem_buf[9],mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xdd,mem_buf,6);
    S2716B_LOG("read 0xdd  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x04,mem_buf,1);
    S2716B_LOG("read 0x04  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x27,mem_buf,1);
    S2716B_LOG("read 0x27  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x28,mem_buf,1);
    S2716B_LOG("read 0x28  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x29,mem_buf,1);
    S2716B_LOG("read 0x29  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x2A,mem_buf,1);
    S2716B_LOG("read 0x2A  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x2B,mem_buf,1);
    S2716B_LOG("read 0x2B  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x2C,mem_buf,1);
    S2716B_LOG("read 0x2C  = %x \n", mem_buf[0]);
    
    gsl_ts_read(client,0x2D,mem_buf,1);
    S2716B_LOG("read 0x2D  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xDD,mem_buf,6);
    S2716B_LOG("read 0xDD  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x05,mem_buf,9);
    S2716B_LOG("read 0x05 = %x %x %x %x %x %x %x %x %x\n",mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x04,mem_buf,1);
    S2716B_LOG("read 0x04  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xDD,mem_buf,6);
    S2716B_LOG("read 0xDD  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x26,mem_buf,1);
    S2716B_LOG("read 0x26  = %x \n", mem_buf[0]);

    gsl_ts_read(client,0x1F,mem_buf,8);
    S2716B_LOG("read 0x1F = %x %x %x %x %x %x %x %x\n",mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x1F,mem_buf,2);
    S2716B_LOG("read 0x1F  = %x %x\n",mem_buf[1],mem_buf[0]);

    gsl_ts_read(client,0x2E,mem_buf,10);
    S2716B_LOG("read 0x2E  = %x %x %x %x  %x %x %x %x %x %x\n",mem_buf[9],mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x20,mem_buf,1);
    S2716B_LOG("read 0x20  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x09,mem_buf,4);
    S2716B_LOG("read 0x09  = %x %x %x %x\n",mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);
    // 初始化输入设备
    init_data[0] = 0x00;
    gsl_ts_write(client,0xFF,init_data,1);

    init_data[0] = 0x00;
    gsl_ts_write(client,0xff,init_data,1);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xDD,mem_buf,6);
    S2716B_LOG("read 0xDD  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x01;
    gsl_ts_write(client,0xff,init_data,1);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x36,mem_buf,25);
    S2716B_LOG("read 0x36  = %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",mem_buf[24],mem_buf[23],mem_buf[22],mem_buf[21],mem_buf[20],mem_buf[19],mem_buf[18],mem_buf[17],mem_buf[16],mem_buf[15],mem_buf[14],mem_buf[13],mem_buf[12],mem_buf[11],mem_buf[10],mem_buf[9],mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x02;
    gsl_ts_write(client,0xFF,init_data,1);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x03;
    gsl_ts_write(client,0xFF,init_data,1);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x04;
    gsl_ts_write(client,0xFF,init_data,1);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,6);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x00;
    gsl_ts_write(client,0xFF,init_data,1);

    gsl_ts_read(client,0x44,mem_buf,1);
    S2716B_LOG("read 0x44  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x45,mem_buf,5);
    S2716B_LOG("read 0x45  = %x %x %x %x %x \n",mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x47,mem_buf,1);
    S2716B_LOG("read 0x47  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x48,mem_buf,3);
    S2716B_LOG("read 0x48  = %x %x %x \n",mem_buf[2], mem_buf[1], mem_buf[0]);

    init_data[0] = 0x07;
    gsl_ts_write(client,0x0e,init_data,1);

    gsl_ts_read(client,0x44,mem_buf,1);
    S2716B_LOG("read 0x44  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x45,mem_buf,5);
    S2716B_LOG("read 0x45  = %x %x %x %x %x \n",mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x47,mem_buf,1);
    S2716B_LOG("read 0x47  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x48,mem_buf,3);
    S2716B_LOG("read 0x48  = %x %x %x \n",mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x16,mem_buf,9);
    S2716B_LOG("read 0x16 = %x %x %x %x %x %x %x %x %x\n",mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);
    
    gsl_ts_write(client,0x16,init_data1,9);

    gsl_ts_read(client,0x19,mem_buf,2);
    S2716B_LOG("read 0x19  = %x %x \n",mem_buf[1], mem_buf[0]);

    gsl_ts_write(client,0x19,init_data2,2);
    
    gsl_ts_read(client,0x13,mem_buf,4);
    S2716B_LOG("read 0x13  = %x %x %x %x \n",mem_buf[3],mem_buf[2],mem_buf[1], mem_buf[0]);

    gsl_ts_write(client,0x13,init_data4,4);

    //gsl_ts_read(client,0x13,mem_buf,4);
    //S2716B_LOG("read 0x13  = %x %x %x %x\n",mem_buf[3],mem_buf[2],mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x1A,mem_buf,1);
    S2716B_LOG("read 0x1A  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x18,mem_buf,20);
    S2716B_LOG("read 0x18 = %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",mem_buf[19],mem_buf[18],mem_buf[17],mem_buf[16],mem_buf[15],mem_buf[14],mem_buf[13],mem_buf[12],mem_buf[11],mem_buf[10],mem_buf[9],mem_buf[8],mem_buf[7],mem_buf[6],mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_write(client,0x18,init_data5,20);

    init_data[0] = 0x02;
    gsl_ts_write(client,0x0F,init_data,1);

    gsl_ts_read(client,0x0D,mem_buf,1);
    S2716B_LOG("read 0x0D  = %x \n",mem_buf[0]);

    init_data[0] = 0x84;
    gsl_ts_write(client,0x0D,init_data,1);

    init_data[0] = 0x04;
    gsl_ts_write(client,0x0D,init_data,1);

    gsl_ts_read(client,0x0D,mem_buf,1);
    S2716B_LOG("read 0x0D  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x19,mem_buf,3);
    S2716B_LOG("read 0x19  = %x %x %x\n",mem_buf[2],mem_buf[1], mem_buf[0]);

    gsl_ts_write(client,0x19,init_data6,3);

    gsl_ts_read(client,0x19,mem_buf,3);
    S2716B_LOG("read 0x19  = %x %x %x\n",mem_buf[2],mem_buf[1], mem_buf[0]);    

    gsl_ts_read(client,0x1B,mem_buf,2);
    S2716B_LOG("read 0x1B  = %x  %x\n",mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE9,mem_buf,6);
    S2716B_LOG("read 0xE9  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0xE3,mem_buf,3);
    S2716B_LOG("read 0xE3  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);
 
    gsl_ts_read(client,0xDD,mem_buf,6);
    S2716B_LOG("read 0xDD  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);
    
    gsl_ts_read(client,0xD7,mem_buf,6);
    S2716B_LOG("read 0xD7  = %x %x %x %x %x %x\n",mem_buf[5],mem_buf[4],mem_buf[3], mem_buf[2], mem_buf[1], mem_buf[0]);

    gsl_ts_read(client,0x04,mem_buf,1);
    S2716B_LOG("read 0x04  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x27,mem_buf,1);
    S2716B_LOG("read 0x27  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x28,mem_buf,1);
    S2716B_LOG("read 0x28  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x29,mem_buf,1);
    S2716B_LOG("read 0x29  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x2A,mem_buf,1);
    S2716B_LOG("read 0x2A  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x2B,mem_buf,1);
    S2716B_LOG("read 0x2B  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x2C,mem_buf,1);
    S2716B_LOG("read 0x2C  = %x \n",mem_buf[0]);

    gsl_ts_read(client,0x2D,mem_buf,1);
    S2716B_LOG("read 0x2D  = %x \n",mem_buf[0]);
#endif
    return 0;
}
static int ctp_get_system_config(void)
{
    char twi_id = 0x01;
    screen_max_x = S2716B_SCREEN_MAX_X;
    screen_max_y = S2716B_SCREEN_MAX_Y;
    revert_x_flag = S2716B_REVERT_X_FLAG;
    revert_y_flag = S2716B_REVERT_Y_FLAG;
    exchange_x_y_flag = S2716B_EXCHANGE_X_Y_FLAG;
    //aaron
    S2716B_LOG("twi_id = %d\n",twi_id);
    S2716B_LOG("screen_max_x = %d\n",screen_max_x);
    S2716B_LOG("screen_max_y = %d\n",screen_max_y);
    S2716B_LOG("revert_x_flag = %d\n",revert_x_flag);
    S2716B_LOG("revert_y_flag = %d\n",revert_y_flag);
    S2716B_LOG("exchange_x_y_flag = %d\n",exchange_x_y_flag);
    if ((screen_max_x == 0) || (screen_max_y == 0)) {
        pr_err("%s:read config error!\n", __func__);
        return 0;
    }
    return 1;
}

static int s2716b_set_gpio(struct touch_device *bdata, int gpio,
		bool config, int dir, int state)
{
	int retval;
	char label[16];

	if (config) {
		retval = snprintf(label, 16, "tcm_gpio_%d\n", gpio);
		if (retval < 0) {
			S2716B_ERR("Failed to set GPIO label\n");
			return retval;
		}

		retval = gpio_request(gpio, label);
		if (retval < 0) {
			S2716B_ERR("Failed to request GPIO %d\n",
					gpio);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval < 0) {
			S2716B_ERR("Failed to set GPIO %d direction\n",
					gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return 0;
}

#ifdef CONFIG_OF
static int s2716b_config_gpio(struct touch_device *bdata)
{
	int retval;

	if (bdata->power_gpio >= 0) {
		retval = s2716b_set_gpio(bdata, bdata->power_gpio,
				true, 1, !bdata->power_on_state);
		if (retval < 0) {
			S2716B_ERR("Failed to configure power GPIO\n");
			goto err_set_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = s2716b_set_gpio(bdata, bdata->reset_gpio,
				true, 1, !bdata->reset_on_state);
		if (retval < 0) {
			S2716B_ERR("Failed to configure reset GPIO\n");
			goto err_set_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_set_gpio_reset:
	if (bdata->power_gpio >= 0)
		s2716b_set_gpio(bdata, bdata->power_gpio, false, 0, 0);

err_set_gpio_power:
	if (bdata->irq_gpio >= 0)
		s2716b_set_gpio(bdata, bdata->irq_gpio, false, 0, 0);

	return retval;
}

static int s2716b_parse_dt(struct device *dev, struct touch_device *bdata)
{
	int retval;
	u32 value;
	struct property *prop;
	struct device_node *np = dev->of_node;
	struct device_node *node = NULL;
	//u32 ints[2] = { 0, 0 };
	
	prop = of_find_property(np, "s2716b_ts,irq-gpio", NULL);
	if (prop && prop->length) {
		bdata->irq_gpio = of_get_named_gpio_flags(np,
				"s2716b_ts,irq-gpio", 0,
				(enum of_gpio_flags *)&bdata->irq_flags);
	} else {
		bdata->irq_gpio = -1;
	}

	retval = of_property_read_u32(np, "s2716b_ts,irq-on-state", &value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;
	
	prop = of_find_property(np, "s2716b_ts,power-gpio", NULL);
	if (prop && prop->length) {
		bdata->power_gpio = of_get_named_gpio_flags(np,
				"s2716b_ts,power-gpio", 0, NULL);
	} else {
		bdata->power_gpio = -1;
	}

	prop = of_find_property(np, "s2716b_ts,power-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "s2716b_ts,power-on-state",
				&value);
		if (retval < 0) {
			S2716B_ERR("Failed to read s2716b_ts,power-on-state property\n");
			return retval;
		} else {
			bdata->power_on_state = value;
		}
	} else {
		bdata->power_on_state = 0;
	}

	prop = of_find_property(np, "s2716b_ts,power-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "s2716b_ts,power-delay-ms",
				&value);
		if (retval < 0) {
			S2716B_ERR("Failed to read s2716b_ts,power-delay-ms property\n");
			return retval;
		} else {
			bdata->power_delay_ms = value;
		}
	} else {
		bdata->power_delay_ms = 0;
	}
	
	prop = of_find_property(np, "s2716b_ts,reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"s2716b_ts,reset-gpio", 0, NULL);
	} else {
		bdata->reset_gpio = -1;
	}

	prop = of_find_property(np, "s2716b_ts,reset-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "s2716b_ts,reset-on-state",
				&value);
		if (retval < 0) {
			S2716B_ERR("Failed to read s2716b_ts,reset-on-state property\n");
			return retval;
		} else {
			bdata->reset_on_state = value;
		}
	} else {
		bdata->reset_on_state = 0;
	}

	prop = of_find_property(np, "s2716b_ts,reset-active-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "s2716b_ts,reset-active-ms",
				&value);
		if (retval < 0) {
			S2716B_ERR("Failed to read s2716b_ts,reset-active-ms property\n");
			return retval;
		} else {
			bdata->reset_active_ms = value;
		}
	} else {
		bdata->reset_active_ms = 0;
	}

	prop = of_find_property(np, "s2716b_ts,reset-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "s2716b_ts,reset-delay-ms",
				&value);
		if (retval < 0) {
			S2716B_ERR("Unable to read s2716b_ts,reset-delay-ms property\n");
			return retval;
		} else {
			bdata->reset_delay_ms = value;
		}
	} else {
		bdata->reset_delay_ms = 0;
	}

	prop = of_find_property(np, "s2716b_ts,tpio-reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->tpio_reset_gpio = of_get_named_gpio_flags(np,
				"s2716b_ts,tpio-reset-gpio", 0, NULL);
	} else {
		bdata->tpio_reset_gpio = -1;
	}
	
	//tpd_gpio_as_int(1);
	node = of_find_matching_node(node, touch_of_match);
    if (node) {
		//of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		//gpio_set_debounce(ints[0], ints[1]);

		touch_dev->irq = irq_of_parse_and_map(node, 0);
		// 注册中断处理函数
		if (request_irq(touch_dev->irq, touch_irq_handler, IRQF_TRIGGER_FALLING,"touchscreen_irq", touch_dev)) {
			S2716B_ERR("Failed to request IRQ\n");
			return -EFAULT;
		} else {
			enable_irq(touch_dev->irq);
			enable_irq_wake(touch_dev->irq);
		}
	}
	
	return 0;
}
#endif

static int touch_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int ret;

	S2716B_LOG("%s:start...\n",__func__); 
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        S2716B_ERR("I2C not supported");
        return -ENODEV;
    }

    if (client->addr != S2716B_I2C_ADDR) {
        S2716B_LOG("[TPD]Change i2c addr 0x%02x to %x",
                 client->addr, S2716B_I2C_ADDR);
        client->addr = S2716B_I2C_ADDR;
        S2716B_LOG("[TPD]i2c addr=0x%x\n", client->addr);
    }

	touch_dev = devm_kzalloc(&client->dev, sizeof(struct touch_device), GFP_KERNEL);
	if (!touch_dev) {
		S2716B_ERR("Failed to allocate memory for board data\n");
		return -ENOMEM;
	}
	touch_dev->client = client;
	touch_dev->tp_flag = 0;
	
	touch_dev->wq = create_singlethread_workqueue("ts_work");
    INIT_WORK(&touch_dev->work, touch_read_data);
    
#ifdef CONFIG_OF
	ret = s2716b_parse_dt(&(touch_dev->client->dev), touch_dev);
	if (ret) {
		S2716B_ERR("Touch s2716b_parse_dt failed\n");
		goto err_init;
	}
	s2716b_config_gpio(touch_dev);
#endif

	if (ctp_get_system_config() != 1) {
        S2716B_ERR("%s:read config fail!\n", __func__);
        ret = -1;
        goto err_init;
    }
    
    ret = touch_init(client);
    if (ret) {
        S2716B_ERR("Touch device init failed\n");
        goto err_init;
    }
    ret = touch_input_init(touch_dev);
    if (ret) {
		S2716B_ERR("Touch input init failed\n");
        goto err_init;
    }

    touch_dev->tp_flag = 1;

	tpd_load_status = 1;
	
    S2716B_LOG("%s:end...\n",__func__); 
    return 0;
    
err_init:
	cancel_work_sync(&touch_dev->work);
	flush_workqueue(touch_dev->wq);
	destroy_workqueue(touch_dev->wq);
	kfree(touch_dev);
	return ret;
}

static int touch_remove(struct i2c_client *client) {
    if (touch_dev->irq > 0) {
        free_irq(touch_dev->irq, touch_dev);
    }

    if (touch_dev && touch_dev->input_dev) {
        input_unregister_device(touch_dev->input_dev);
    }
    kfree(touch_dev);
    return 0;
}

#ifdef CONFIG_OF
static struct of_device_id s2716b_touch_of_match[] = {
	{ .compatible = "allwinner,egalax_i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, s2716b_touch_of_match);
#else
#define s2716b_touch_of_match NULL
#endif

static const struct i2c_device_id touch_id_table[] = {
	{CTP_NAME, 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, touch_id_table);

static struct i2c_driver touch_driver = {
	.driver = {
		.name = CTP_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = s2716b_touch_of_match,
#endif
	},
	.probe = touch_probe,
	.remove = touch_remove,
	.id_table = touch_id_table,
};

static int tpd_local_init(void)
{
    int retval;

    S2716B_LOG("s2716b tcm driver %s: entry\n", __func__);
    retval = i2c_add_driver(&touch_driver);
	if(retval) {
		S2716B_ERR("failed to add i2c driver");
		goto err_driver;
	}

	S2716B_LOG("finished\n");

err_driver:
	return retval;
}

static void s2716b_mtk_resume(struct device *dev)
{
	int ret = 0;
	struct touch_device *bdata = touch_dev;
	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
		msleep(bdata->reset_delay_ms);
	}
	
	ret = touch_init(bdata->client);
    if (ret) {
        S2716B_ERR("Touch device init failed\n");
    }
	mdelay(20);
	enable_irq(bdata->irq);
}

static void s2716b_mtk_suspend(struct device *dev)
{
	struct touch_device *bdata = touch_dev;
	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
		msleep(bdata->reset_active_ms);
	}
	
	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, !bdata->power_on_state);
		msleep(bdata->power_delay_ms);
	}

	disable_irq(bdata->irq);
}


static unsigned char firmware_version[50];
static char *s2716b_ts_get_tp_firmware_version(void)
{
	snprintf(firmware_version, sizeof(firmware_version), "Tpfirmware version:not support\n");
	return firmware_version;	
}

struct tpd_driver_t s2716b_mtk_driver = {
	.tpd_device_name = "s2716b_ts",
	.tpd_local_init = tpd_local_init,
	.suspend = s2716b_mtk_suspend,
	.resume = s2716b_mtk_resume,
	.tpd_have_button = 0,
	.get_tp_firmware_version=s2716b_ts_get_tp_firmware_version,
};


int s2716b_ts_module_init(void)
{
	tpd_get_dts_info();
    
	if(tpd_driver_add(&s2716b_mtk_driver) < 0){
		S2716B_ERR("Fail to add s2716b tpd driver\n");
		return -ENODEV;
	}    

	return 0;
}

void s2716b_ts_module_exit(void)
{
	i2c_del_driver(&touch_driver);
	return;
}
