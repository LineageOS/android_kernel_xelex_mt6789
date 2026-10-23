/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef PANEL_Q30_HD1440_LCM_DSI_VDO
#define PANEL_Q30_HD1440_LCM_DSI_VDO


#define REGFLAG_CMD             0xFFFA
#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define FRAME_WIDTH                 1440
#define FRAME_HEIGHT                1440

#define PHYSICAL_WIDTH              80000
#define PHYSICAL_HEIGHT             80000

#define PLL_CLK                     386

#define HSA                         4
#define HBP                         30
#define HFP                         125
#define VSA                         4
#define VBP                         16
#define VFP                         30

#define FPS                  60


/*Parameter setting for mode 0 End*/


#endif //end of PANEL_Q30_HD1440_LCM_DSI_VDO
