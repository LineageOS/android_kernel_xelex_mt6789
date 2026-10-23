/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _AW95016_H_
#define _AW95016_H_

#define AW95016_ID 0x80
#define AW95016_KEY_PORT_MAX (0x10) /* 16 */
#define AW95016_INT_MASK (0xFFFF)

#define AWINIC_DEBUG		1

#ifdef AWINIC_DEBUG
#define AW_DEBUG(fmt, args...)	pr_info(fmt, ##args)
#else
#define AW_DEBUG(fmt, ...)
#endif



/*register list old*/
#define P0_INPUT		0x00
#define P1_INPUT		0x01
#define P0_OUTPUT		0x02
#define P1_OUTPUT		0x03

#define P0_DIR			0x06
#define P1_DIR			0x07
#define P0_MAK			0x12
#define P1_MAK			0x13
#define P0_DOMD			0x16
#define P1_DOMD			0x17

#define RESET			0x70

struct aw95016_key;
struct aw95016_gpio;

struct aw95016 {
	struct i2c_client *client;
	struct device *dev;
	int irq_gpio;
	int irq_num;
	int rst_gpio;
	struct regulator *power_supply;
	unsigned char chipid;
	bool matrix_key_enable;
	bool single_key_enable;
	bool gpio_feature_enable;
	bool screen_state;
	struct aw95016_key *key_data;
	struct aw95016_gpio *gpio_data;
	int kb_blk_gpio;
	struct led_classdev kb_led_cdev;
};

typedef struct {
	char name[10];
	int key_code;
	int key_val;
} KEY_STATE;

unsigned int aw95016_separate_key_data[AW95016_KEY_PORT_MAX] = {
/*      0    1    2    3 */
	1,   2,   3,   4,
	5,   6,   7,   8,
	9,   10,  11,  12,
	13,  14,  15,  16
};

struct aw95016_key {
	unsigned int key_mask;
	unsigned int input_port_nums;
	unsigned int output_port_nums;
	unsigned int input_port_mask;
	unsigned int output_port_mask;
	unsigned int new_input_state;
	unsigned int old_input_state;
	unsigned int *new_output_state;
	unsigned int *old_output_state;
	unsigned int *def_output_state;
	bool wake_up_enable;
	struct input_dev *input;

	unsigned int debounce_delay;
	struct delayed_work int_work;
	struct hrtimer key_timer;
	struct work_struct key_work;
	KEY_STATE *keymap;
	int keymap_len;
	struct aw95016 *priv;
};

enum aw95016_gpio_dir {
	AW95016_GPIO_INPUT = 0,
	AW95016_GPIO_OUTPUT = 1,
};

enum aw95016_gpio_val {
	AW95016_GPIO_HIGH = 1,
	AW95016_GPIO_LOW = 0,
};

enum aw95016_gpio_output_mode {
	AW95016_OPEN_DRAIN_OUTPUT = 0,
	AW95016_PUSH_PULL_OUTPUT = 1,
};

struct aw95016_singel_gpio {
	unsigned int gpio_idx;
	enum aw95016_gpio_dir gpio_direction;
	enum aw95016_gpio_val state;
	struct aw95016 *priv;
};


struct aw95016_gpio {
	unsigned int gpio_mask;
	unsigned int gpio_num;
	enum aw95016_gpio_output_mode output_mode;
	struct aw95016_singel_gpio *single_gpio_data;
};
#endif
