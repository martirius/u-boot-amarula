/*
 * Copyright (C) 2013 Boundary Devices Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __ASM_ARCH_IMX6_PINS_H__
#define __ASM_ARCH_IMX6_PINS_H__

#include <asm/mach-imx/iomux-v3.h>

#define IMX6_PAD_DECLARE(prefix, name, pco, mc, mm, sio, si, pc) \
	prefix##name = IOMUX_PAD(pco, mc, mm, sio, si, pc)

#ifdef CONFIG_IMX6QDL
enum {
#define IMX6_PAD_DECL(name, pco, mc, mm, sio, si, pc) \
	IMX6_PAD_DECLARE(IMX6Q_PAD_,name, pco, mc, mm, sio, si, pc),
#include "imx6q_pins.h"
#undef IMX6_PAD_DECL
#define IMX6_PAD_DECL(name, pco, mc, mm, sio, si, pc) \
	IMX6_PAD_DECLARE(IMX6DL_PAD_,name, pco, mc, mm, sio, si, pc),
#include "imx6dl_pins.h"
};
#elif defined(CONFIG_IMX6Q)
enum {
#define IMX6_PAD_DECL(name, pco, mc, mm, sio, si, pc) \
	IMX6_PAD_DECLARE(IMX6_PAD_,name, pco, mc, mm, sio, si, pc),
#include "imx6q_pins.h"
};
#elif defined(CONFIG_IMX6DL) || defined(CONFIG_IMX6S)
enum {
#define IMX6_PAD_DECL(name, pco, mc, mm, sio, si, pc) \
	IMX6_PAD_DECLARE(IMX6_PAD_,name, pco, mc, mm, sio, si, pc),
#include "imx6dl_pins.h"
};
#elif defined(CONFIG_IMX6SLL)
#include "imx6sll_pins.h"
#elif defined(CONFIG_IMX6SL)
#include "imx6sl_pins.h"
#elif defined(CONFIG_IMX6SX)
#include "imx6sx_pins.h"
#elif defined(CONFIG_IMX6ULL)
#include "imx6ull_pins.h"
#elif defined(CONFIG_IMX6UL)
#include "imx6ul_pins.h"
#else
#error "Please select cpu"
#endif	/* CONFIG_IMX6Q */

#endif	/*__ASM_ARCH_IMX6_PINS_H__ */
