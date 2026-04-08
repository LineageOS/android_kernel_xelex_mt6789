// SPDX-License-Identifier: GPL-2.0-only
/*
 * Keyboard Driver for Blackberry Keyboards BBQ10 from arturo182. Software written by wallComputer.
 * bbqX0kbd_main.c: Main C File.
 */

#include "bbqX0kbd_main.h"

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
static atomic_t keepWorking = ATOMIC_INIT(1);
static struct workqueue_struct *workqueue_struct;
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
/* hh: use disp_notifier */
static struct notifier_block bbqX0kpd_disp_notifier;
static int  bbqX0kbd_disp_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
/* use disp_notifier */
#endif

static int q20_switch_key_mouse_debug_read(struct seq_file *m, void *v);
static ssize_t q20_switch_key_mouse_debug_write(struct file *filp, const char  *buff, size_t len, loff_t *data);
static int hodafone_q20_switch_key_mouse_open(struct inode *inode, struct file *file);
static int q20_spec_power_flag_debug_read(struct seq_file *m, void *v);
static ssize_t q20_spec_power_flag_debug_write(struct file *filp, const char  *buff, size_t len, loff_t *data);
static int hodafone_q20_spec_power_flag_open(struct inode *inode, struct file *file);

static const struct proc_ops q20_switch_key_mouse_proc_ops = {
	.proc_open  = hodafone_q20_switch_key_mouse_open,
	.proc_read  = seq_read,
    .proc_write = q20_switch_key_mouse_debug_write,
    .proc_release = single_release,
};

static const struct proc_ops q20_spec_power_flag_proc_ops = {
	.proc_open  = hodafone_q20_spec_power_flag_open,
	.proc_read  = seq_read,
    .proc_write = q20_spec_power_flag_debug_write,
    .proc_release = single_release,
};


static int hodafone_q20_switch_key_mouse_open(struct inode *inode, struct file *file)
{
	return single_open(file, q20_switch_key_mouse_debug_read, NULL);
};

static int q20_switch_key_mouse_debug_read(struct seq_file *m, void *v)
{
	if (g_bbqX0kbd_data) {
		seq_printf(m, "%d\n", g_bbqX0kbd_data->q20_spec_switch_key_mouse);
	}
	return 0;
}

static ssize_t q20_switch_key_mouse_debug_write(struct file *filp, const char  *buff, size_t len, loff_t *data)
{
	unsigned char writebuf[50];	
	if (copy_from_user(&writebuf, buff, len)) {
		return -EFAULT;
	}
	
	if (g_bbqX0kbd_data == NULL) {
		return -EFAULT;
	}

    BBQX0KBD_KEYBOARD_LOG("HodafoneLog q20_switch_key_mouse_debug_write writebuf=%s",writebuf);
   	if(writebuf[0] == '0'){
		g_bbqX0kbd_data->q20_spec_switch_key_mouse = 0;
		g_bbqX0kbd_data->keycode[0x05] = BTN_LEFT;
	}else if(writebuf[0] == '1'){
		g_bbqX0kbd_data->q20_spec_switch_key_mouse = 1;
		g_bbqX0kbd_data->keycode[0x05] = KEY_ENTER;
	} 
	return len;
}

static int hodafone_q20_spec_power_flag_open(struct inode *inode, struct file *file)
{
	return single_open(file, q20_spec_power_flag_debug_read, NULL);
};

static int q20_spec_power_flag_debug_read(struct seq_file *m, void *v)
{
	if (g_bbqX0kbd_data) {
		seq_printf(m, "%d\n", g_bbqX0kbd_data->q20_spec_power_flag);
	}
	return 0;
}

static ssize_t q20_spec_power_flag_debug_write(struct file *filp, const char  *buff, size_t len, loff_t *data)
{
	unsigned char writebuf[50];	
	if (copy_from_user(&writebuf, buff, len)) {
		return -EFAULT;
	}
	
	if (g_bbqX0kbd_data == NULL) {
		return -EFAULT;
	}

    BBQX0KBD_KEYBOARD_LOG("HodafoneLog q20_spec_power_flag_debug_write writebuf=%s",writebuf);
   	if(writebuf[0] == '0'){
		g_bbqX0kbd_data->q20_spec_power_flag = 0;
	}else if(writebuf[0] == '1'){
		g_bbqX0kbd_data->q20_spec_power_flag = 1;
	} 
	return len;
}

static uint8_t bbqX0kbd_modkeys_to_bits(unsigned short mod_keycode)
{
	uint8_t returnValue;

	switch (mod_keycode) {
	case (KEY_LEFTCTRL):
		returnValue = LEFT_CTRL_BIT;
		break;
	case KEY_RIGHTCTRL:
		returnValue = RIGHT_CTRL_BIT;
		break;
	case KEY_LEFTSHIFT:
		returnValue = LEFT_SHIFT_BIT;
		break;
	case KEY_RIGHTSHIFT:
		returnValue = RIGHT_SHIFT_BIT;
		break;
	case KEY_LEFTALT:
		returnValue = LEFT_ALT_BIT;
		break;
	case KEY_RIGHTALT:
		returnValue = RIGHT_ALT_BIT;
		break;
	//TODO: Add for GUI Key if needed.
	}
	return returnValue;
}

static void bbqX0kbd_inject_shift()
{
	if (g_bbqX0kbd_data) {
		input_event(g_bbqX0kbd_data->input_dev, EV_MSC, MSC_SCAN, 0x700e1);
		input_report_key(g_bbqX0kbd_data->input_dev, KEY_LEFTSHIFT, 1);
		input_sync(g_bbqX0kbd_data->input_dev);
		input_event(g_bbqX0kbd_data->input_dev, EV_MSC, MSC_SCAN, 0x700e1);
		input_report_key(g_bbqX0kbd_data->input_dev, KEY_LEFTSHIFT, 0);
		input_sync(g_bbqX0kbd_data->input_dev);
	}
}

static unsigned short bbqX0kbd_get_num_lock_keycode(unsigned short keycode)
{
	unsigned short returnValue;

	switch (keycode) {
	case KEY_Q:
		bbqX0kbd_inject_shift();
		returnValue = KEY_3;
		break;
	case KEY_W:
		returnValue = KEY_1;
		break;
	case KEY_E:
		returnValue = KEY_2;
		break;
	case KEY_R:
		returnValue = KEY_3;
		break;
	case KEY_T:
		bbqX0kbd_inject_shift();
		returnValue = KEY_9;
		break;
	case KEY_Y:
		bbqX0kbd_inject_shift();
		returnValue = KEY_0;
		break;
	case KEY_U:
		bbqX0kbd_inject_shift();
		returnValue = KEY_MINUS;
		break;
	case KEY_I:
		returnValue = KEY_MINUS;
		break;
	case KEY_O:
		bbqX0kbd_inject_shift();
		returnValue = KEY_EQUAL;
		break;
	case KEY_P:
		bbqX0kbd_inject_shift();
		returnValue = KEY_2;
		break;
	case KEY_A:
		bbqX0kbd_inject_shift();
		returnValue = KEY_8;
		break;
	case KEY_S:
		returnValue = KEY_4;
		break;
	case KEY_D:
		returnValue = KEY_5;
		break;
	case KEY_F:
		returnValue = KEY_6;
		break;
	case KEY_G:
		returnValue = KEY_SLASH;
		break;
	case KEY_H:
		bbqX0kbd_inject_shift();
		returnValue = KEY_SEMICOLON;
		break;
	case KEY_J:
		returnValue = KEY_SEMICOLON;
		break;
	case KEY_K:
		returnValue = KEY_APOSTROPHE;
		break;
	case KEY_L:
		bbqX0kbd_inject_shift();
		returnValue = KEY_APOSTROPHE;
		break;
	case KEY_Z:
		returnValue = KEY_7;
		break;
	case KEY_X:
		returnValue = KEY_8;
		break;
	case KEY_C:
		returnValue = KEY_9;
		break;
	case KEY_V:
		bbqX0kbd_inject_shift();
		returnValue = KEY_SLASH;
		break;
	case KEY_B:
		bbqX0kbd_inject_shift();
		returnValue = KEY_1;
		break;
	case KEY_N:
		returnValue = KEY_COMMA;
		break;
	case KEY_M:
		returnValue = KEY_DOT;
		break;
	case KEY_GRAVE:
		returnValue = KEY_0;
		break;
	default:
		returnValue = keycode;
		break;
	}
	return returnValue;
}

static unsigned short bbqX0kbd_get_altgr_keycode(unsigned short keycode)
{
	unsigned short returnValue;

	switch (keycode) {
	case KEY_E:
		returnValue = KEY_PAGEDOWN;
		break;
	case KEY_R:
		returnValue = KEY_PAGEUP;
		break;
	case KEY_Y:
		returnValue = KEY_UP;
		break;
	case KEY_G:
		returnValue = KEY_LEFT;
		break;
	case KEY_H:
		returnValue = KEY_HOME;
		break;
	case KEY_J:
		returnValue = KEY_RIGHT;
		break;
	case KEY_B:
		returnValue = KEY_DOWN;
		break;
	case KEY_M:
		returnValue = KEY_MENU;
		break;
	case KEY_K:
		returnValue = KEY_VOLUMEUP;
		break;
	case KEY_L:
		returnValue = KEY_VOLUMEDOWN;
		break;
	case KEY_GRAVE:
		returnValue = KEY_MUTE;
		break;
	case KEY_BACKSPACE:
		returnValue = KEY_DELETE;
		break;
	default:
		returnValue = keycode;
		break;
	}
	return returnValue;
}

static void bbqX0kbd_set_brightness(struct bbqX0kbd_data *bbqX0kbd_data, unsigned short keycode, uint8_t *reportKey)
{
	uint8_t swapVar;

	switch (keycode) {
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	case KEY_Q:
		*reportKey = 1;
		if (bbqX0kbd_data->screenBrightness > 0xFF - BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->screenBrightness = 0xFF;
		else
			bbqX0kbd_data->screenBrightness = bbqX0kbd_data->screenBrightness + BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_W:
		*reportKey = 1;
		if (bbqX0kbd_data->screenBrightness < BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->screenBrightness = 0;
		else
			bbqX0kbd_data->screenBrightness = bbqX0kbd_data->screenBrightness - BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_S:
		*reportKey = 1;
		swapVar = bbqX0kbd_data->screenBrightness;
		bbqX0kbd_data->screenBrightness = (bbqX0kbd_data->screenBrightness == 0) ? bbqX0kbd_data->lastScreenBrightness : 0;
		bbqX0kbd_data->lastScreenBrightness = swapVar;
		break;
#endif
	case KEY_Z:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness > 0xFF - BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0xFF;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness + BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_X:
		*reportKey = 0;
		if (bbqX0kbd_data->keyboardBrightness < BBQ10_BRIGHTNESS_DELTA)
			bbqX0kbd_data->keyboardBrightness = 0;
		else
			bbqX0kbd_data->keyboardBrightness = bbqX0kbd_data->keyboardBrightness - BBQ10_BRIGHTNESS_DELTA;
		break;
	case KEY_0:
		*reportKey = 0;
		swapVar = bbqX0kbd_data->keyboardBrightness;
		bbqX0kbd_data->keyboardBrightness = (bbqX0kbd_data->keyboardBrightness == 0) ? bbqX0kbd_data->lastKeyboardBrightness : 0;
		bbqX0kbd_data->lastKeyboardBrightness = swapVar;
		break;
	default:
		*reportKey = 2;
		break;
	}
	if (*reportKey == 0)
		bbqX0kbd_write(bbqX0kbd_data->i2c_client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &bbqX0kbd_data->keyboardBrightness, sizeof(uint8_t));

#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	if (*reportKey == 1)
		bbqX0kbd_write(bbqX0kbd_data->i2c_client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));
#endif
}

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
static void bbqX0kbd_work_handler(struct work_struct *work_struct)
{
	struct bbqX0kbd_data *bbqX0kbd_data;
	struct i2c_client *i2c_client;
	uint8_t fifoData[2];
	uint8_t registerValue;
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	uint8_t registerX, registerY;
#endif
	uint8_t count;
	uint8_t reportKey = 2;
	int returnValue;
	unsigned short keycode;

	if (atomic_read(&keepWorking) == 1) {
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
		BBQX0KBD_KEYBOARD_LOG("%s Done with Queue.\n", __func__);
#endif
		return;
	}

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		BBQX0KBD_KEYBOARD_LOG("%s Doing Queue now.\n", __func__);
#endif
	bbqX0kbd_data = container_of(work_struct, struct bbqX0kbd_data, delayed_work.work);
	i2c_client = bbqX0kbd_data->i2c_client;

	returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0) {
		BBQX0KBD_KEYBOARD_ERR("%s : Could not read REG_INT. Error: %d\n", __func__, returnValue);
		return;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	BBQX0KBD_KEYBOARD_LOG("%s Interrupt: 0x%02x\n", __func__, registerValue);
#endif
	if (registerValue == 0x00) {
		queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
		return;
	}

	if (registerValue & REG_INT_OVERFLOW)
		BBQX0KBD_KEYBOARD_LOG("%s overflow occurred.\n", __func__);

	if (registerValue & REG_INT_KEY) {
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_KEY, &count, sizeof(uint8_t));

		if (returnValue != 0) {
			BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_KEY, Error: %d\n", __func__, returnValue);
			queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
			return;
		}

		count = count & REG_KEY_KEYCOUNT_MASK;
		while (count) {
			returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_FIF, fifoData, 2*sizeof(uint8_t));
			if (returnValue != 0) {
				BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_FIF, Error: %d\n", __func__, returnValue);
				queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
				return;
			}
			if (fifoData[0] == KEY_PRESSED_STATE || fifoData[0] == KEY_RELEASED_STATE) {
				input_event(bbqX0kbd_data->input_dev, EV_MSC, MSC_SCAN, fifoData[1]);

				keycode = bbqX0kbd_data->keycode[fifoData[1]];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				BBQX0KBD_KEYBOARD_LOG("%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, fifoData[1], fifoData[1], keycode, fifoData[0], reportKey);
#endif
				switch (keycode) {
				case KEY_UNKNOWN:
					BBQX0KBD_KEYBOARD_LOG("%s Could not get Keycode for Scancode: [0x%02X]\n", __func__, fifoData[1]);
					break;
				case KEY_RIGHTSHIFT:
					if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_data->lockStatus ^= NUMS_LOCK_BIT;
					fallthrough;
				case KEY_LEFTSHIFT:
				case KEY_RIGHTALT:
				case KEY_LEFTALT:
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
					if (fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_data->modifier_keys_status |= bbqX0kbd_modkeys_to_bits(keycode);
					else
						bbqX0kbd_data->modifier_keys_status &= ~bbqX0kbd_modkeys_to_bits(keycode);
					fallthrough;
				default:
					if (keycode == KEY_LEFTALT)	/* Left alt is only used for symbols */
						reportKey = 255;	/*   so never generate events for it */
					else
						reportKey = 2;
					if (bbqX0kbd_data->lockStatus & NUMS_LOCK_BIT || bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT)
						keycode = bbqX0kbd_get_num_lock_keycode(keycode);
					else if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT)
						keycode = bbqX0kbd_get_altgr_keycode(keycode);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
					else if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && fifoData[1] == 0x05 && (bbqX0kbd_data->q20_spec_switch_key_mouse == 0))
						keycode = BTN_RIGHT;
#endif
					if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT && fifoData[0] == KEY_PRESSED_STATE)
						bbqX0kbd_set_brightness(bbqX0kbd_data, keycode, &reportKey);
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
					BBQX0KBD_KEYBOARD_LOG("%s AFTER : MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, fifoData[1], fifoData[1],  keycode, fifoData[0], reportKey);
#endif
					if (reportKey == 2)
						input_report_key(bbqX0kbd_data->input_dev, keycode, fifoData[0] == KEY_PRESSED_STATE);
					break;
				}
			}
			--count;
		}
		input_sync(bbqX0kbd_data->input_dev);
		registerValue = 0x00;
		returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
		if (returnValue < 0)
			BBQX0KBD_KEYBOARD_ERR("%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
	}

#if (BBQX0KBD_TYPE ==  BBQ20KBD_PMOD)
	if (registerValue & REG_INT_TOUCH) {

		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_TOX, &registerX, sizeof(uint8_t));
		if (returnValue < 0) {
			BBQX0KBD_KEYBOARD_ERR("%s : Could not read REG_TOX. Error: %d\n", __func__, returnValue);
			return;
		}
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_TOY, &registerY, sizeof(uint8_t));
		if (returnValue < 0) {
			BBQX0KBD_KEYBOARD_ERR("%s : Could not read REG_TOY. Error: %d\n", __func__, returnValue);
			return;
		}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		BBQX0KBD_KEYBOARD_LOG("%s X Reg: %d Y Reg: %d.\n", __func__, (int8_t)registerX, (int8_t)registerY);
#endif
		if (bbqX0kbd_data->q20_spec_switch_key_mouse == 0)
		{
			input_report_rel(bbqX0kbd_data->input_dev, REL_X, (int8_t)registerX);
			input_report_rel(bbqX0kbd_data->input_dev, REL_Y, (int8_t)registerY);
			input_sync(bbqX0kbd_data->input_dev);
			registerValue = 0x00;
			returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
			if (returnValue < 0)
				BBQX0KBD_KEYBOARD_ERR("%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
		}
	}

#endif

	queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
}

static int bbqX0kbd_queue_work(struct bbqX0kbd_data *bbqX0kbd_data)
{
	INIT_DELAYED_WORK(&bbqX0kbd_data->delayed_work, bbqX0kbd_work_handler);
	atomic_set(&keepWorking, 0);
	return queue_delayed_work(bbqX0kbd_data->workqueue_struct, &bbqX0kbd_data->delayed_work, msecs_to_jiffies(bbqX0kbd_data->work_rate_ms));
}
#endif


#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
static void bbqX0kbd_read_fifo(struct bbqX0kbd_data *bbqX0kbd_data)
{
	struct i2c_client *i2c_client = bbqX0kbd_data->i2c_client;
	uint8_t fifo_data[2];
	uint8_t count;
	uint8_t pos;
	int returnValue;

	returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_KEY, &count, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_KEY, Error: %d\n", __func__, returnValue);
		return;
	}
	count = count & REG_KEY_KEYCOUNT_MASK;
	bbqX0kbd_data->fifoCount = count;
	pos = 0;
	while (count) {
		returnValue = bbqX0kbd_read(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_FIF, fifo_data, 2*sizeof(uint8_t));
		if (returnValue != 0) {
			BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_FIF, Error: %d\n", __func__, returnValue);
			return;
		}
		bbqX0kbd_data->fifoData[pos][0] = fifo_data[0];
		bbqX0kbd_data->fifoData[pos][1] = fifo_data[1];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		BBQX0KBD_KEYBOARD_LOG("%s Filled Data: KeyState:%d SCANCODE:%d at Pos: %d Count: %d\n",
			__func__, bbqX0kbd_data->fifoData[pos][0], bbqX0kbd_data->fifoData[pos][1], pos, count);
#endif
		++pos;
		--count;
	}
}

static void bbqX0kbd_work_fnc(struct work_struct *work_struct_ptr)
{
	struct bbqX0kbd_data *bbqX0kbd_data;
	struct input_dev *input_dev;
	struct i2c_client *i2c_client;
	unsigned short keycode;

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	int8_t registerX, registerY;
	unsigned short touch_keycode = 0;
#endif

	uint8_t pos = 0;
	uint8_t reportKey = 2;
	uint8_t registerValue = 0x00;
	int returnValue = 0;

	bbqX0kbd_data = container_of(work_struct_ptr, struct bbqX0kbd_data, work_struct);
	input_dev = bbqX0kbd_data->input_dev;
	i2c_client = bbqX0kbd_data->i2c_client;

	while (bbqX0kbd_data->fifoCount > 0) {
		if (bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE || bbqX0kbd_data->fifoData[pos][0] == KEY_RELEASED_STATE) {
			input_event(input_dev, EV_MSC, MSC_SCAN, bbqX0kbd_data->fifoData[pos][1]);

			keycode = bbqX0kbd_data->keycode[bbqX0kbd_data->fifoData[pos][1]];
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				BBQX0KBD_KEYBOARD_LOG("%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, bbqX0kbd_data->fifoData[pos][1], bbqX0kbd_data->fifoData[pos][1], keycode, bbqX0kbd_data->fifoData[pos][0], reportKey);
#endif
			switch (keycode) {
			case KEY_UNKNOWN:
				BBQX0KBD_KEYBOARD_LOG("%s Could not get Keycode for Scancode: [0x%02X]\n", __func__, bbqX0kbd_data->fifoData[pos][1]);
				break;
			case KEY_RIGHTSHIFT:
				if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_data->lockStatus ^= NUMS_LOCK_BIT;
				fallthrough;
			case KEY_LEFTSHIFT:
			case KEY_RIGHTALT:
			case KEY_LEFTALT:
			case KEY_LEFTCTRL:
			case KEY_RIGHTCTRL:
				if (bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_data->modifier_keys_status |= bbqX0kbd_modkeys_to_bits(keycode);
				else
					bbqX0kbd_data->modifier_keys_status &= ~bbqX0kbd_modkeys_to_bits(keycode);
				fallthrough;
			default:
				if (keycode == KEY_LEFTALT)	/* Left alt is only used for symbols */
					reportKey = 255;	/*   so never generate events for it */
				else
					reportKey = 2;
				if (bbqX0kbd_data->lockStatus & NUMS_LOCK_BIT || bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT)
					keycode = bbqX0kbd_get_num_lock_keycode(keycode);
				else if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT)
					keycode = bbqX0kbd_get_altgr_keycode(keycode);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
				else if (bbqX0kbd_data->modifier_keys_status & LEFT_ALT_BIT && bbqX0kbd_data->fifoData[pos][1] == 0x05 && (bbqX0kbd_data->q20_spec_switch_key_mouse == 0))
					keycode = BTN_RIGHT;
#endif
				if (bbqX0kbd_data->modifier_keys_status & RIGHT_ALT_BIT && bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE)
					bbqX0kbd_set_brightness(bbqX0kbd_data, keycode, &reportKey);
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
				BBQX0KBD_KEYBOARD_LOG("%s BEFORE: MODKEYS: 0x%02X LOCKKEYS: 0x%02X scancode: %d(%c) keycode: %d State: %d reportKey: %d\n", __func__, bbqX0kbd_data->modifier_keys_status, bbqX0kbd_data->lockStatus, bbqX0kbd_data->fifoData[pos][1], bbqX0kbd_data->fifoData[pos][1], keycode, bbqX0kbd_data->fifoData[pos][0], reportKey);
#endif
				if (reportKey == 2)
					input_report_key(input_dev, keycode, bbqX0kbd_data->fifoData[pos][0] == KEY_PRESSED_STATE);
				break;
			}
		}
		++pos;
		--bbqX0kbd_data->fifoCount;
	}

#if (BBQX0KBD_TYPE ==  BBQ20KBD_PMOD)
	if (bbqX0kbd_data->touchInt) {
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
		BBQX0KBD_KEYBOARD_LOG("%s X Reg: %d Y Reg: %d.\n", __func__, bbqX0kbd_data->rel_x, bbqX0kbd_data->rel_y);
#endif
		registerX = bbqX0kbd_data->rel_x;
		registerY = bbqX0kbd_data->rel_y;
		if (bbqX0kbd_data->q20_spec_switch_key_mouse == 0) {
			input_report_rel(input_dev, REL_X, (int8_t)registerX);
			input_report_rel(input_dev, REL_Y, (int8_t)registerY);
		} else if (bbqX0kbd_data->q20_spec_switch_key_mouse == 1){
			bbqX0kbd_data->relX_offset += registerX;
			bbqX0kbd_data->relY_offset += registerY;
	
			BBQX0KBD_KEYBOARD_LOG("%s:relX_offset= %d; relY_offset= %d\n",__func__,bbqX0kbd_data->relX_offset,bbqX0kbd_data->relY_offset);
			// 判断滑动方向
			if (abs(bbqX0kbd_data->relY_offset) > BBQ20KBD_TRACKPAD_KEYS_OFFSET) {// 垂直滑动
				BBQX0KBD_KEYBOARD_LOG("------>registerY = %d\n",bbqX0kbd_data->relY_offset);
				if (bbqX0kbd_data->relY_offset > 0) {
					touch_keycode = KEY_DOWN; //0x6c
				} else {
					touch_keycode = KEY_UP;  //0x67
				}
			}
			
			if(abs(bbqX0kbd_data->relX_offset) > BBQ20KBD_TRACKPAD_KEYS_OFFSET) {// 水平滑动
				BBQX0KBD_KEYBOARD_LOG("------>registerX = %d\n",bbqX0kbd_data->relX_offset);
				if (bbqX0kbd_data->relX_offset > 0) {
					touch_keycode = KEY_RIGHT;  //0x6a
				} else {
					touch_keycode = KEY_LEFT; //0x69
				}
			}
			
			if(touch_keycode > 0) {
				BBQX0KBD_KEYBOARD_LOG("------>key code = %x\n",touch_keycode);
				input_report_key(input_dev, touch_keycode, 1);  // 按下
				input_report_key(input_dev, touch_keycode, 0);  // 释放
				bbqX0kbd_data->relY_offset = 0;
				bbqX0kbd_data->relX_offset = 0;
			}
		}
		
		bbqX0kbd_data->touchInt = 0;
	} else {
		bbqX0kbd_data->relX_offset = 0;
		bbqX0kbd_data->relY_offset = 0;
	}
#endif

	input_sync(input_dev);
	registerValue = 0x00;
	returnValue = bbqX0kbd_write(i2c_client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0)
		BBQX0KBD_KEYBOARD_ERR("%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
	enable_irq(i2c_client->irq);
}

static irqreturn_t bbqX0kbd_irq_handler(int irq, void *dev_id)
{
	struct bbqX0kbd_data *bbqX0kbd_data = dev_id;
	struct i2c_client *client = bbqX0kbd_data->i2c_client;
	int returnValue;
	uint8_t registerValue;

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	BBQX0KBD_KEYBOARD_LOG("%s Interrupt Fired. IRQ: %d\n", __func__, irq);
#endif
	disable_irq_nosync(client->irq);
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0) {
		BBQX0KBD_KEYBOARD_ERR("%s: Could not read REG_INT. Error: %d\n", __func__, returnValue);
		enable_irq(client->irq);
		return IRQ_NONE;
	}

#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	BBQX0KBD_KEYBOARD_LOG("%s Interrupt: 0x%02x\n", __func__, registerValue);
#endif

	if (registerValue == 0x00) {
		enable_irq(client->irq);
		return IRQ_NONE;
	}

	if (registerValue & REG_INT_OVERFLOW)
		dev_warn(&client->dev, "%s overflow occurred.\n", __func__);

	if (registerValue & REG_INT_KEY) {
		bbqX0kbd_read_fifo(bbqX0kbd_data);
		schedule_work(&bbqX0kbd_data->work_struct);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
		if(bbqX0kbd_data->q20_spec_switch_key_mouse == 1) {
			return IRQ_HANDLED;
		}
#endif
	}
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	if (registerValue & REG_INT_TOUCH) {
		returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_TOX, &registerValue, sizeof(uint8_t));
		if (returnValue < 0) {
			BBQX0KBD_KEYBOARD_ERR("%s : Could not read REG_TOX. Error: %d\n", __func__, returnValue);
			enable_irq(client->irq);
			return IRQ_NONE;
		}
		bbqX0kbd_data->rel_x = (int8_t)registerValue;
		returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_TOY, &registerValue, sizeof(uint8_t));
		if (returnValue < 0) {
			BBQX0KBD_KEYBOARD_ERR("%s : Could not read REG_TOY. Error: %d\n", __func__, returnValue);
			enable_irq(client->irq);
			return IRQ_NONE;
		}
		bbqX0kbd_data->rel_y = (int8_t)registerValue;
		bbqX0kbd_data->touchInt = 1;
		schedule_work(&bbqX0kbd_data->work_struct);
	} else
		bbqX0kbd_data->touchInt = 0;
#endif

	return IRQ_HANDLED;
}
#endif

static int bbqX0kbd_keyboard_probe_dt(struct device_node * np, struct bbqX0kbd_data *bbqX0kbd_data)
{
	struct property *prop;

    if (!np) {
        BBQX0KBD_KEYBOARD_ERR("ERROR! get keyboard failed, func:%s, line:%d\n", __func__, __LINE__);
    }

	prop = of_find_property(np, "irq-gpio", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->irq_gpio = of_get_named_gpio_flags(np,
				"int-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->irq_gpio = -1;
	}

	prop = of_find_property(np, "reset-gpios", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->rst_gpio = of_get_named_gpio_flags(np,
				"reset-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->rst_gpio = -1;
	}
	
	prop = of_find_property(np, "power-vdd-gpios", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->power_vdd_gpio = of_get_named_gpio_flags(np,
				"power-vdd-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->power_vdd_gpio = -1;
	}
	
	prop = of_find_property(np, "power-1v8-gpios", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->power_1v8_gpio = of_get_named_gpio_flags(np,
				"power-1v8-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->power_1v8_gpio = -1;
	}
	
	prop = of_find_property(np, "power-2v8-gpios", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->power_2v8_gpio = of_get_named_gpio_flags(np,
				"power-2v8-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->power_2v8_gpio = -1;
	}
	
	prop = of_find_property(np, "firmware-download-gpios", NULL);
	if (prop && prop->length) {
		bbqX0kbd_data->firmware_download_gpio = of_get_named_gpio_flags(np,
				"firmware-download-gpios", 0, NULL);
	} else {
		bbqX0kbd_data->firmware_download_gpio = -1;
	}

    return 0;
}

static int bbqX0kbd_keypoard_set_gpio(int gpio,
		bool config, int dir, int state)
{
	int retval;
	char label[16];

	if (config) {
		retval = snprintf(label, 16, "q20_gpio_%d\n", gpio);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to set GPIO label\n");
			return retval;
		}

		retval = gpio_request(gpio, label);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to request GPIO %d\n",
					gpio);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to set GPIO %d direction\n",
					gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return 0;
}


static int bbqX0kbd_power_enable(struct bbqX0kbd_data *bbqX0kbd_data, u32 enable)
{
	int retval;
    
    if (bbqX0kbd_data->power_vdd_gpio >= 0) {
		retval = bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_vdd_gpio,
				true, 1, 0);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to configure power vdd GPIO\n");
			goto err_set_gpio_vdd_power;
		}
	}
	if (bbqX0kbd_data->power_1v8_gpio >= 0) {
		retval = bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_1v8_gpio,
				true, 1, 0);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to configure power 1v8 GPIO\n");
			goto err_set_gpio_1v8_power;
		}
	}
	if (bbqX0kbd_data->power_2v8_gpio >= 0) {
		retval = bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_2v8_gpio,
				true, 1, 0);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to configure power 2v8 GPIO\n");
			goto err_set_gpio_2v8_power;
		}
	}
	
	if (bbqX0kbd_data->firmware_download_gpio >= 0) {
		retval = bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->firmware_download_gpio,
				true, 1, 0);
		if (retval < 0) {
			BBQX0KBD_KEYBOARD_ERR("Failed to configure power firmware download GPIO\n");
		}
	}
	
	if (bbqX0kbd_data->power_vdd_gpio >= 0) {
		gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 1);
	}
	mdelay(10);
	if (bbqX0kbd_data->power_1v8_gpio >= 0) {
		gpio_set_value(bbqX0kbd_data->power_1v8_gpio, 1);
	}
	
	if (bbqX0kbd_data->power_2v8_gpio >= 0) {
		gpio_set_value(bbqX0kbd_data->power_2v8_gpio, 1);
	}
	
	if (bbqX0kbd_data->firmware_download_gpio >= 0) {
		gpio_set_value(bbqX0kbd_data->firmware_download_gpio, 0);
	}
    mdelay(10);
err_set_gpio_2v8_power:
	if (bbqX0kbd_data->power_2v8_gpio >= 0)
		bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_2v8_gpio, false, 0, 0);

err_set_gpio_1v8_power:
	if (bbqX0kbd_data->power_1v8_gpio >= 0)
		bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_1v8_gpio, false, 0, 0);
		
err_set_gpio_vdd_power:
	if (bbqX0kbd_data->power_vdd_gpio >= 0)
		bbqX0kbd_keypoard_set_gpio(bbqX0kbd_data->power_vdd_gpio, false, 0, 0);
    return 0;
}

/*add by hodafone begin*/
static int firmware_upgrade_debug_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gpio_get_value(g_bbqX0kbd_data->firmware_download_gpio));
	return 0;
}

static int firmware_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, firmware_upgrade_debug_read, NULL);
};

static ssize_t firmware_upgrade_debug_write(struct file *filp, const char  *buff, size_t len, loff_t *data)
{
	struct bbqX0kbd_data *bbqX0kbd_data = g_bbqX0kbd_data;
	unsigned char writebuf[50];	
	if (copy_from_user(&writebuf, buff, len)) {
		return -EFAULT;
	}

    BBQX0KBD_KEYBOARD_ERR("HodafoneLog firmware_upgrade_debug_write writebuf=%s",writebuf);
   	if(writebuf[0] == '0'){
		//trun off 
		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 0);
		}
		
		if (bbqX0kbd_data->firmware_download_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->firmware_download_gpio, 0);
		}
		mdelay(100);
		
		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 1);
		}
	} else if(writebuf[0] == '1'){
		//trun off 
		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 0);
		}
		
	} else if(writebuf[0] == '2'){
		//firmware upgrade
		if (bbqX0kbd_data->firmware_download_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->firmware_download_gpio, 1);
		}

		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 1);
		}
	}
	return len;
}

static const struct proc_ops firmware_upgrade_proc_ops = {
	.proc_open  = firmware_upgrade_open,
	.proc_read  = seq_read,
    .proc_write = firmware_upgrade_debug_write,
    .proc_release = single_release,
};

/*add by hodafone end*/

static int bbqX0kbd_resume_register_init(struct i2c_client *client)
{
	int returnValue;
	uint8_t registerValue = 0x00;
	
	registerValue = REG_CFG_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write configuration to BBQX0KBD. Error: %d\n", __func__, returnValue);
	}
	
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_CFG. Error: %d\n", __func__, returnValue);
	} else {
		BBQX0KBD_KEYBOARD_LOG("%s Configuration Register Value: 0x%02X\n", __func__, registerValue);
	}
#endif	

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	registerValue = REG_CFG2_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write configuration 2 to BBQX0KBD. Error: %d\n", __func__, returnValue);
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_CF2. Error: %d\n", __func__, returnValue);
	} else {
		BBQX0KBD_KEYBOARD_LOG("%s Configuration 2 Register Value: 0x%02X\n", __func__, registerValue);
	}
#endif
#endif
		
	return 0;
}

static int bbqX0kbd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
#if BBQX0KBD_REGISTE_PLATFORM_DRIVER
	struct bbqX0kbd_data *bbqX0kbd_data = g_bbqX0kbd_data;
#else
	struct bbqX0kbd_data *bbqX0kbd_data;
#endif
	struct input_dev *input;
	int returnValue;
	int i;
	uint8_t registerValue = 0x00;

#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	BBQX0KBD_KEYBOARD_LOG("%s Probing BBQX0KBD.\n", __func__);
#endif
#if BBQX0KBD_REGISTE_PLATFORM_DRIVER
	if (!bbqX0kbd_data)
		return -ENOMEM;
	bbqX0kbd_data->i2c_client = client;
	i2c_set_clientdata(client, bbqX0kbd_data);
#else
	bbqX0kbd_data = devm_kzalloc(dev, sizeof(*bbqX0kbd_data), GFP_KERNEL);
	if (!bbqX0kbd_data)
		return -ENOMEM;
	g_bbqX0kbd_data = bbqX0kbd_data;
	bbqX0kbd_data->i2c_client = client;
	i2c_set_clientdata(client, bbqX0kbd_data);
	memcpy(bbqX0kbd_data->keycode, keycodes, sizeof(bbqX0kbd_data->keycode));
	
	bbqX0kbd_keyboard_probe_dt(client->dev.of_node, bbqX0kbd_data);
	
	/*add by hodafone begin*/
	fwu_proc_entry = proc_create(PROC_NAME, 0777, NULL,&firmware_upgrade_proc_ops);
	/*add by hodafone end*/

	bbqX0kbd_power_enable(bbqX0kbd_data, 1);
#endif	
	msleep(400);
	
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_RST, &registerValue, sizeof(uint8_t));
	if (returnValue) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not Reset BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
	msleep(400);

	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_VER, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not Read Version BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
	BBQX0KBD_KEYBOARD_LOG("%s BBQX0KBD Software version: 0x%02X\n", __func__, registerValue);
	bbqX0kbd_data->version_number = registerValue;

	registerValue = REG_CFG_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write configuration to BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CFG, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_CFG. Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	BBQX0KBD_KEYBOARD_LOG("%s Configuration Register Value: 0x%02X\n", __func__, registerValue);
#endif

#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	registerValue = REG_CFG2_DEFAULT_SETTING;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write configuration 2 to BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENODEV;
	}
#if (DEBUG_LEVEL & DEBUG_LEVEL_LD)
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_CF2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read REG_CF2. Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	BBQX0KBD_KEYBOARD_LOG("%s Configuration 2 Register Value: 0x%02X\n", __func__, registerValue);
#endif
#endif


#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	if (BBQX0KBD_POLL_PERIOD < BBQX0KBD_MINIMUM_WORK_RATE || BBQX0KBD_POLL_PERIOD > BBQX0KBD_MAXIMUM_WORK_RATE)
		bbqX0kbd_data->work_rate_ms = BBQX0KBD_DEFAULT_WORK_RATE;
	else
		bbqX0kbd_data->work_rate_ms = BBQX0KBD_POLL_PERIOD;
#endif

#if (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_MOUSE)
	bbqX0kbd_data->q20_spec_switch_key_mouse = 0;
	bbqX0kbd_data->keycode[0x05] = BTN_LEFT;
#elif (BBQ20KBD_TRACKPAD_USE == BBQ20KBD_TRACKPAD_AS_KEYS)
	bbqX0kbd_data->q20_spec_switch_key_mouse = 1;
	bbqX0kbd_data->keycode[0x05] = KEY_ENTER;
#endif	

	fwu_proc_entry = proc_create(PROC_NAME, 0777, NULL,&firmware_upgrade_proc_ops);

	input = devm_input_allocate_device(dev);
	if (!input) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not devm_input_allocate_device BBQX0KBD. Error: %d\n", __func__, returnValue);
		return -ENOMEM;
	}
	bbqX0kbd_data->input_dev = input;

	input->name = DEVICE_NAME;
	input->id.bustype = BBQX0KBD_BUS_TYPE;
	input->id.vendor  = BBQX0KBD_VENDOR_ID;
	input->id.product = BBQX0KBD_PRODUCT_ID;
	input->id.version = BBQX0KBD_VERSION_ID;
	input->keycode = bbqX0kbd_data->keycode;
	input->keycodesize = sizeof(bbqX0kbd_data->keycode[0]);
	input->keycodemax = ARRAY_SIZE(bbqX0kbd_data->keycode);

	for (i = 0; i < NUM_KEYCODES; i++)
		__set_bit(bbqX0kbd_data->keycode[i], input->keybit);

	__clear_bit(KEY_RESERVED, input->keybit);

	__set_bit(EV_REP, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	input_set_capability(input, EV_MSC, MSC_SCAN);
#if (BBQX0KBD_TYPE == BBQ20KBD_PMOD)
	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
#endif

	bbqX0kbd_data->modifier_keys_status = 0x00; // Serendipitously coincides with idle state of all keys.
	bbqX0kbd_data->lockStatus = 0x00;
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	bbqX0kbd_data->screenBrightness = 0xFF;
	bbqX0kbd_data->lastScreenBrightness = bbqX0kbd_data->screenBrightness;
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &bbqX0kbd_data->screenBrightness, sizeof(uint8_t));
#endif
	bbqX0kbd_data->keyboardBrightness = 0xFF;
	bbqX0kbd_data->lastKeyboardBrightness = bbqX0kbd_data->keyboardBrightness;
	bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &bbqX0kbd_data->keyboardBrightness, sizeof(uint8_t));

#if (BBQX0KBD_INT == BBQX0KBD_USE_INT)
	returnValue = devm_request_threaded_irq(dev, client->irq,
										NULL, bbqX0kbd_irq_handler,
										IRQF_SHARED | IRQF_ONESHOT,
										client->name, bbqX0kbd_data);

	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("Coudl not claim IRQ %d; error %d\n", client->irq, returnValue);
		return returnValue;
	}
	INIT_WORK(&bbqX0kbd_data->work_struct, bbqX0kbd_work_fnc);
#endif

#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	bbqX0kbd_data->workqueue_struct = create_singlethread_workqueue("bbqX0kbd_workqueue");
	if (bbqX0kbd_data->workqueue_struct == NULL) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not create_singlethreaded_workqueue.", __func__);
		return -ENOMEM;
	}
	workqueue_struct = bbqX0kbd_data->workqueue_struct;
	bbqX0kbd_queue_work(bbqX0kbd_data);
#endif
	returnValue = input_register_device(input);
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("Failed to register input device, error: %d\n", returnValue);
		return returnValue;
	}
	
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	bbqX0kpd_disp_notifier.notifier_call = bbqX0kbd_disp_notifier_callback;
	if (mtk_disp_notifier_register("bbqX0kbd", &bbqX0kpd_disp_notifier))
		BBQX0KBD_KEYBOARD_LOG("Failed to register disp notifier client:%d");
#endif

	q20_switch_key_mouse_proc_entry = proc_create(Q20_SWITCH_KEY_MOUSE_PROC_NAME, 0777, NULL, &q20_switch_key_mouse_proc_ops);
    if (NULL == q20_switch_key_mouse_proc_entry) 
    {
        BBQX0KBD_KEYBOARD_LOG("proc_create q20_switch_key_mouse_proc_entry error:\n");
    }
    else
    {
        BBQX0KBD_KEYBOARD_LOG("proc_create q20_switch_key_mouse_proc_entry    Succesfull*****\n");
    }
    
    q20_spec_power_flag_proc_entry = proc_create(Q20_SPEC_POWER_FLAG_PROC_NAME, 0777, NULL, &q20_spec_power_flag_proc_ops);
    if (NULL == q20_spec_power_flag_proc_entry) 
    {
        BBQX0KBD_KEYBOARD_LOG("proc_create q20_spec_power_flag_proc_entry error:\n");
    }
    else
    {
        BBQX0KBD_KEYBOARD_LOG("proc_create q20_spec_power_flag_proc_entry    Succesfull*****\n");
    }
    
	device_init_wakeup(&client->dev, false);

	return 0;
}

static void bbqX0kbd_shutdown(struct i2c_client *client)
{
	int returnValue;
	uint8_t registerValue = 0x00;
#if (DEBUG_LEVEL & DEBUG_LEVEL_FE)
	BBQX0KBD_KEYBOARD_LOG("%s Shutting Down Keyboard And Screen Backlight.\n", __func__);
#endif
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write to BBQX0KBD Backlight. Error: %d\n", __func__, returnValue);
		return;
	}
	returnValue = bbqX0kbd_read(client, BBQX0KBD_I2C_ADDRESS, REG_VER, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not read BBQX0KBD Software Version. Error: %d\n", __func__, returnValue);
		return;
	}
#if (BBQX0KBD_TYPE == BBQ10KBD_FEATHERWING)
	registerValue = 0x00;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BK2, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write to BBQX0KBD Screen Backlight. Error: %d\n", __func__, returnValue);
		return;
	}
#endif

}

#ifdef CONFIG_PM_SLEEP
static int bbqX0kbd_suspend(struct device *dev)
{
	int returnValue;
	uint8_t registerValue = 0x00;
    struct i2c_client *client = to_i2c_client(dev);
    struct bbqX0kbd_data *bbqX0kbd_data = i2c_get_clientdata(client);
    
    bbqX0kbd_data->suspended = true;
    
    /* Turn off backlight to save power */
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &registerValue, sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write to BBQX0KBD Backlight. Error: %d\n", __func__, returnValue);
		return 0;
	}
	mdelay(10);
    
    if (device_may_wakeup(dev) || bbqX0kbd_data->q20_spec_power_flag) {
        enable_irq_wake(client->irq);
    } else {
        disable_irq(client->irq);
		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 0);
		}
    }
    
    return 0;
}

static int bbqX0kbd_resume(struct device *dev)
{
	int returnValue;
	uint8_t registerValue = 0x00;
    struct i2c_client *client = to_i2c_client(dev);
    struct bbqX0kbd_data *bbqX0kbd_data = i2c_get_clientdata(client);
    
    
    if (device_may_wakeup(dev) || bbqX0kbd_data->q20_spec_power_flag) {
        disable_irq_wake(client->irq);
    } else {
		if (bbqX0kbd_data->power_vdd_gpio >= 0) {
			gpio_set_value(bbqX0kbd_data->power_vdd_gpio, 1);
		}
		mdelay(400);
		
		bbqX0kbd_resume_register_init(client);
		
        enable_irq(client->irq);
	}

	/* Restore backlight level */
	bbqX0kbd_data->keyboardBrightness = 0xFF;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_BKL, &(bbqX0kbd_data->keyboardBrightness), sizeof(uint8_t));
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not write to BBQX0KBD Backlight. Error: %d\n", __func__, returnValue);
	}
    
    bbqX0kbd_data->suspended = false;
    
    registerValue = 0x00;
	returnValue = bbqX0kbd_write(client, BBQX0KBD_I2C_ADDRESS, REG_INT, &registerValue, sizeof(uint8_t));
	if (returnValue < 0)
		BBQX0KBD_KEYBOARD_ERR("%s Could not clear REG_INT. Error: %d\n", __func__, returnValue);
    
    return 0;
}
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
int  bbqX0kbd_disp_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
	int *evdata = (int *)data;
	struct bbqX0kbd_data *bbqX0kbd_data = g_bbqX0kbd_data;
	if (event == MTK_DISP_EVENT_BLANK) {
		BBQX0KBD_KEYBOARD_LOG("%s IN", __func__);
		if (*evdata == MTK_DISP_BLANK_UNBLANK) {
			if (bbqX0kbd_data->suspended) {
				bbqX0kbd_resume(&(bbqX0kbd_data->i2c_client->dev));
			}
		}
		BBQX0KBD_KEYBOARD_LOG("%s OUT", __func__);
	} else if (event == MTK_DISP_EARLY_EVENT_BLANK) {
		BBQX0KBD_KEYBOARD_LOG("%s IN", __func__);
		if (*evdata == MTK_DISP_BLANK_POWERDOWN) {
			if (!bbqX0kbd_data->suspended) {
				bbqX0kbd_suspend(&(bbqX0kbd_data->i2c_client->dev));
			}
		}
		BBQX0KBD_KEYBOARD_LOG("%s OUT", __func__);
	}
	return 0;
}
#endif
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#else 
static SIMPLE_DEV_PM_OPS(bbqX0kbd_pm_ops, bbqX0kbd_suspend, bbqX0kbd_resume);
#endif


static struct i2c_driver bbqX0kbd_driver = {
	.driver = {
		.name = DEVICE_NAME,
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#else
		.pm = &bbqX0kbd_pm_ops,
#endif
		.of_match_table = bbqX0kbd_of_device_id,
	},
	.probe		= bbqX0kbd_probe,
	.shutdown	= bbqX0kbd_shutdown,
	.id_table	= bbqX0kbd_i2c_device_id,
};

#if BBQX0KBD_REGISTE_PLATFORM_DRIVER
static int bbqX0kdb_pdrv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bbqX0kbd_data *bbqX0kbd_data;
	int returnValue;
	
	bbqX0kbd_data = devm_kzalloc(dev, sizeof(*bbqX0kbd_data), GFP_KERNEL);
	if (!bbqX0kbd_data)
		return -ENOMEM;
	g_bbqX0kbd_data = bbqX0kbd_data;
	memcpy(bbqX0kbd_data->keycode, keycodes, sizeof(bbqX0kbd_data->keycode));
	
	bbqX0kbd_keyboard_probe_dt(pdev->dev.of_node, bbqX0kbd_data);
	/*add by hodafone begin*/
	fwu_proc_entry = proc_create(PROC_NAME, 0777, NULL,&firmware_upgrade_proc_ops);
	/*add by hodafone end*/

	bbqX0kbd_power_enable(bbqX0kbd_data, 1);

	returnValue = i2c_add_driver(&bbqX0kbd_driver);
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not initialise BBQX0KBD driver! Error: %d\n", __func__, returnValue);
	}
	BBQX0KBD_KEYBOARD_LOG("%s Initalised BBQX0KBD.\n", __func__);
	
	return 0;
}

static struct platform_driver bbqX0kdb_pdrv_driver = {
	.probe = bbqX0kdb_pdrv_probe,
	.driver = {
		   .name = "bbqX0kdb_pdrv",
		   .of_match_table = bbqX0kbd_pdrv_device_id,
	},
};
#endif
static int __init bbqX0kbd_init(void)
{
#if BBQX0KBD_REGISTE_PLATFORM_DRIVER
	return platform_driver_register(&bbqX0kdb_pdrv_driver);
#else
	int returnValue;

	returnValue = i2c_add_driver(&bbqX0kbd_driver);
	if (returnValue != 0) {
		BBQX0KBD_KEYBOARD_ERR("%s Could not initialise BBQX0KBD driver! Error: %d\n", __func__, returnValue);
		return returnValue;
	}
	BBQX0KBD_KEYBOARD_LOG("%s Initalised BBQX0KBD.\n", __func__);
	return returnValue;
#endif
}
module_init(bbqX0kbd_init);

static void __exit bbqX0kbd_exit(void)
{
	BBQX0KBD_KEYBOARD_LOG("%s Exiting BBQX0KBD.\n", __func__);
#if (BBQX0KBD_INT == BBQX0KBD_NO_INT)
	atomic_set(&keepWorking, 1);
	flush_workqueue(workqueue_struct);
	msleep(500);
	destroy_workqueue(workqueue_struct);
#endif
	i2c_del_driver(&bbqX0kbd_driver);
#if BBQX0KBD_REGISTE_PLATFORM_DRIVER
	platform_driver_unregister(&bbqX0kdb_pdrv_driver);
#endif
}
module_exit(bbqX0kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wallComputer");
MODULE_DESCRIPTION("Keyboard driver for BBQ10, hardware by arturo182 <arturo182@tlen.pl>, software by wallComputer.");
