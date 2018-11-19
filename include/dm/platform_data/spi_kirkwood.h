/*
 * Copyright (C) 2018 Jagan Teki <jagan@amarulasolutions.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __spi_kirkwood_h
#define __spi_kirkwood_h

struct mvebu_spi_platdata {
	struct kwspi_registers *spireg;
	bool is_errata_50mhz_ac;
};

#endif /* __spi_kirkwood_h */
