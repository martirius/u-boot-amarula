// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 */

#include <common.h>
#include <fdtdec.h>
#include <malloc.h>
#include <spi.h>

int spi_set_wordlen(struct spi_slave *slave, unsigned int wordlen)
{
	if (wordlen == 0 || wordlen > 32) {
		printf("spi: invalid wordlen %u\n", wordlen);
		return -1;
	}

	slave->wordlen = wordlen;

	return 0;
}

void *spi_do_alloc_slave(int offset, int size, unsigned int bus,
			 unsigned int cs)
{
	u8 *ptr;

	ptr = malloc(size);
	if (ptr) {
		struct spi_slave *slave;

		memset(ptr, '\0', size);
		slave = (struct spi_slave *)(ptr + offset);
		slave->bus = bus;
		slave->cs = cs;
		slave->wordlen = SPI_DEFAULT_WORDLEN;
	}

	return ptr;
}

#if !defined(CONFIG_DM_SPI) && \
	defined(CONFIG_DAVINCI_SPI) || \
	defined(CONFIG_KIRKWOOD_SPI) || \
	defined(CONFIG_MPC8XXX_SPI) || \
	defined(CONFIG_TI_QSPI)
void spi_cs_activate(struct spi_slave *slave)
{
}

void spi_cs_deactivate(struct spi_slave *slave)
{
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
				  unsigned int max_hz, unsigned int mode)
{
	return NULL;
}

void spi_free_slave(struct spi_slave *slave)
{
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen,
	     const void *dout, void *din, unsigned long flags)
{
	return 0;
}

int spi_claim_bus(struct spi_slave *slave)
{
	return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
}
#endif
