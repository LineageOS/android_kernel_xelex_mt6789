// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "include/panel-q30-hd1440-lcm-dsi-vdo.h"
#endif

/*add by hodafone begin*/
#include <linux/proc_fs.h> 
#include <linux/seq_file.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#define PROC_NAME	"hodafone_lcm"
static struct proc_dir_entry *lcm_proc_entry;
/*add by hodafone end*/

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define LCM_NAME "panel-q30_hd1440_lcm_dsi_vdo"

struct hodafone_lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio, *disp_pwm_gpio, *vci_gpio;

	bool prepared;
	bool enabled;
	
	char *board_name;

	int error;
};

static struct hodafone_lcm *mlocalctx = NULL;

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct hodafone_lcm *panel_to_hodafone_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct hodafone_lcm, panel);
}
#if 0
#define HODAFONE_Q30_V1_BOARD_NAME "q30_v1"
int is_board_q30_v1(struct hodafone_lcm *ctx) {
	if (ctx->board_name && (0 == strcmp(HODAFONE_Q20_V1_BOARD_NAME, ctx->board_name))) {
		
		return 1;
	}

	return 0;
}
#endif

static void lcm_dcs_write(struct hodafone_lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct hodafone_lcm *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(60);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(200);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(60);
	
	lcm_dcs_write_seq_static(ctx, 0x53,0x2c);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x2a,0x00,0x00,0x05,0x9f);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x2b,0x00,0x00,0x05,0x9f);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x36,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x3a,0x77);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x51,0x80);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x55,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x5e,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x35,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx,0x11);	//sleep out
	mdelay(125);
	lcm_dcs_write_seq_static(ctx,0x29);	//display on
	mdelay(10);
	
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct hodafone_lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct hodafone_lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_err(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static int lcm_disable(struct drm_panel *panel)
{
	struct hodafone_lcm *ctx = panel_to_hodafone_lcm(panel);

	pr_err("%s\n", __func__);
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct hodafone_lcm *ctx = panel_to_hodafone_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	//int ret;

	if (!ctx->prepared)
		return 0;

	pr_err("%s\n", __func__);
	mipi_dsi_dcs_set_tear_off(dsi);
	mdelay(10);
	mipi_dsi_dcs_set_display_off(dsi);
	mdelay(10);
	mipi_dsi_dcs_enter_sleep_mode(dsi);
	mdelay(150);

	gpiod_set_value(ctx->reset_gpio, 0);
	
	ctx->error = 0;
	ctx->prepared = false;
	
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct hodafone_lcm *ctx = panel_to_hodafone_lcm(panel);
	//struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;
	

	pr_err("%s\n", __func__);
	if (ctx->prepared)
		return 0;
	
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct hodafone_lcm *ctx = panel_to_hodafone_lcm(panel);

	pr_err("%s\n", __func__);
	if (ctx->enabled)
		return 0;
	
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}


#define PCLK_MODE0_IN_KHZ \
      ((FRAME_WIDTH + HFP + HSA + HBP) * (FRAME_HEIGHT + VFP + VSA + VBP) * FPS / 1000)

static const struct drm_display_mode default_mode = {
	.clock = PCLK_MODE0_IN_KHZ,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP,
	.vsync_end = FRAME_HEIGHT + VFP + VSA,
	.vtotal = FRAME_HEIGHT + VFP + VSA + VBP,
};


#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.pll_clk = PLL_CLK,
	.data_rate = PLL_CLK*2,
	.data_rate_khz = PLL_CLK*2*1000,
	.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.ssc_enable = 0,
	.phy_timcon = {
		.clk_hs_post = 0x23,
		.clk_hs_prpr = 0x2a,
	},
	.dyn = {
		.switch_en = 1,
		.pll_clk = PLL_CLK,
	},
	//.cmd_null_pkt_en = 1,
    //.cmd_null_pkt_len = 200,
    //.lp_perline_en = 1,
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct hodafone_lcm *ctx = panel_to_hodafone_lcm(panel);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	pr_err("%s level %d\n", __func__,level);
	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
};
#endif

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 80;
	connector->display_info.height_mm = 80;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};


/*add by hodafone begin*/
static int lcm_debug_read(struct seq_file *m, void *v)                     
{
	seq_printf(m, "%s\n", LCM_NAME);
	return 0;
}

static int proc_hodafone_lcm_open(struct inode *inode, struct file *file)
{
	return single_open(file, lcm_debug_read, NULL);
};

static struct proc_ops hodafone_lcm_fops = {
	.proc_open  = proc_hodafone_lcm_open,
	.proc_read  = seq_read,
};

/*add by hodafone end*/

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct hodafone_lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct hodafone_lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	mlocalctx = ctx;
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET 
					| MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);

	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	/*
	 * GPIO84 is configured as DISP_PWM function via pinmux,
	 * so we don't need to request it as a regular GPIO here.
	 * The PWM signal will be output through the pinmux configuration.
	 */
	ctx->disp_pwm_gpio = devm_gpiod_get_optional(dev, "disp_pwm", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->disp_pwm_gpio)) {
		dev_info(dev, "%s: disp_pwm gpio not available (using pinmux), err=%ld\n",
				__func__, PTR_ERR(ctx->disp_pwm_gpio));
		ctx->disp_pwm_gpio = NULL;
	}
	
	ctx->vci_gpio = devm_gpiod_get(dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(dev, "%s: cannot get vci gpio %ld\n",
				__func__, PTR_ERR(ctx->vci_gpio));
	}
	
	ctx->board_name = (char *)of_get_property(dev->of_node, "board_name", NULL);
	
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);

	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	/*add by hodafone begin*/
	lcm_proc_entry = proc_create(PROC_NAME, 0777, NULL,&hodafone_lcm_fops);
	/*add by hodafone end*/
	
	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct hodafone_lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	/*add by hodafone begin*/
	if (lcm_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
	/*add by hodafone end*/
	
	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "mediatek,q30_panel_general,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver q30_lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = LCM_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(q30_lcm_driver);

MODULE_AUTHOR("Tai-Hua Tseng <tai-hua.tseng@mediatek.com>");
MODULE_DESCRIPTION("hodafonehw_panel_general vdo Panel Driver");
MODULE_LICENSE("GPL v2");
