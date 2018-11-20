/*
 * Copyright (C) 2018 Jagan Teki <jagan@amarulasolutions.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __spi_ti_qspi_h
#define __spi_ti_qspi_h

struct ti_qspi_platdata {
	struct ti_qspi_regs *base;
	void *ctrl_mod_mmap;
	ulong fclk;
	void *memory_map;
	uint max_hz;
	u32 num_cs;
};

#endif /* __spi_ti_qspi_h */
