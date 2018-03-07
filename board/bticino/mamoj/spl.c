/*
 * Copyright (C) 2018 Simone CIANNI <simone.cianni@bticino.it>
 * Copyright (C) 2018 Raffaele RECALCATI <raffaele.recalcati@bticino.it>
 * Copyright (C) 2018 Jagan Teki <jagan@amarulasolutions.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>

#include <asm/io.h>
#include <linux/sizes.h>

#include <asm/arch/clock.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL	(PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED | \
			PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

static iomux_v3_cfg_t const uart3_pads[] = {
	IOMUX_PADS(PAD_EIM_D24__UART3_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_EIM_D25__UART3_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static int mx6dl_dcd_table[] = {
	0x020e0774, 0x000C0000, /* MX6_IOM_GRP_DDR_TYPE */
	0x020e0754, 0x00000000, /* MX6_IOM_GRP_DDRPKE */

	0x020e04ac, 0x00000028,	/* MX6_IOM_DRAM_SDCLK_0 */
	0x020e04b0, 0x00000028,	/* MX6_IOM_DRAM_SDCLK_1 */

	0x020e0464, 0x00000028,	/* MX6_IOM_DRAM_CAS */
	0x020e0490, 0x00000028,	/* MX6_IOM_DRAM_RAS */
	0x020e074c, 0x00000028,	/* MX6_IOM_GRP_ADDDS */

	0x020e0494, 0x00000028,	/* MX6_IOM_DRAM_RESET */
	0x020e04a0, 0x00000000,	/* MX6_IOM_DRAM_SDBA2 */
	0x020e04b4, 0x00000028,	/* MX6_IOM_DRAM_SDODT0 */
	0x020e04b8, 0x00000028,	/* MX6_IOM_DRAM_SDODT1 */
	0x020e076c, 0x00000028,	/* MX6_IOM_GRP_CTLDS */

	0x020e0750, 0x00020000,	/* MX6_IOM_GRP_DDRMODE_CTL */
	0x020e04bc, 0x00000028,	/* MX6_IOM_DRAM_SDQS0 */
	0x020e04c0, 0x00000028,	/* MX6_IOM_DRAM_SDQS1 */
	0x020e04c4, 0x00000028,	/* MX6_IOM_DRAM_SDQS2 */
	0x020e04c8, 0x00000028,	/* MX6_IOM_DRAM_SDQS3 */

	0x020e0760, 0x00020000,	/* MX6_IOM_GRP_DDRMODE */
	0x020e0764, 0x00000028,	/* MX6_IOM_GRP_B0DS */
	0x020e0770, 0x00000028,	/* MX6_IOM_GRP_B1DS */
	0x020e0778, 0x00000028,	/* MX6_IOM_GRP_B2DS */
	0x020e077c, 0x00000028,	/* MX6_IOM_GRP_B3DS */

	0x020e0470, 0x00000028,	/* MX6_IOM_DRAM_DQM0 */
	0x020e0474, 0x00000028,	/* MX6_IOM_DRAM_DQM1 */
	0x020e0478, 0x00000028,	/* MX6_IOM_DRAM_DQM2 */
	0x020e047c, 0x00000028,	/* MX6_IOM_DRAM_DQM3 */

	0x021b001c, 0x00008000,	/* MMDC0_MDSCR */

	0x021b0800, 0xA1390003,	/* DDR_PHY_P0_MPZQHWCTRL */

	0x021b080c, 0x0042004b, /* MMDC1_MPWLDECTRL0 */
	0x021b0810, 0x0038003c,	/* MMDC1_MPWLDECTRL1 */

	0x021b083c, 0x42340230,	/* MPDGCTRL0 PHY0 */
	0x021b0840, 0x0228022c,	/* MPDGCTRL1 PHY0 */

	0x021b0848, 0x42444646,	/* MPRDDLCTL PHY0 */

	0x021b0850, 0x38382e2e,	/* MPWRDLCTL PHY0 */

	0x021b081c, 0x33333333,	/* DDR_PHY_P0_MPREDQBY0DL3 */
	0x021b0820, 0x33333333,	/* DDR_PHY_P0_MPREDQBY1DL3 */
	0x021b0824, 0x33333333,	/* DDR_PHY_P0_MPREDQBY2DL3 */
	0x021b0828, 0x33333333,	/* DDR_PHY_P0_MPREDQBY3DL3 */

	0x021b08b8, 0x00000800,	/* DDR_PHY_P0_MPMUR0 */

	0x021b0004, 0x0002002D,	/* MMDC0_MDPDC */
	0x021b0008, 0x00333040,	/* MMDC0_MDOTC */
	0x021b000c, 0x3F4352F3,	/* MMDC0_MDCFG0 */
	0x021b0010, 0xB66D8B63,	/* MMDC0_MDCFG1 */
	0x021b0014, 0x01FF00DB,	/* MMDC0_MDCFG2 */

	0x021b0018, 0x00011740,	/* MMDC0_MDMISC */
	0x021b001c, 0x00008000,	/* MMDC0_MDSCR */
	0x021b002c, 0x000026D2,	/* MMDC0_MDRWD */
	0x021b0030, 0x00431023,	/* MMDC0_MDOR */
	0x021b0040, 0x00000017,	/* Chan0 CS0_END */
	0x021b0000, 0x83190000,	/* MMDC0_MDCTL */

	0x021b001c, 0x02008032,	/* MMDC0_MDSCR  MR2 write, CS0 */
	0x021b001c, 0x00008033,	/* MMDC0_MDSCR, MR3 write, CS0 */
	0x021b001c, 0x00048031,	/* MMDC0_MDSCR, MR1 write, CS0 */
	0x021b001c, 0x15208030,	/* MMDC0_MDSCR, MR0write, CS0 */
	0x021b001c, 0x04008040,	/* MMDC0_MDSCR */

	0x021b0020, 0x00007800,	/* MMDC0_MDREF */

	0x021b0818, 0x00022227,	/* DDR_PHY_P0_MPODTCTRL */

	0x021b0004, 0x0002556D,	/* MMDC0_MDPDC */
	0x021b0404, 0x00011006,	/* MMDC0_MAPSR ADOPT */
	0x021b001c, 0x00000000,	/* MMDC0_MDSCR */
};

static void ccgr_init(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0x00003f3f, &ccm->CCGR0);
	writel(0x0030fc00, &ccm->CCGR1);
	writel(0x000fc000, &ccm->CCGR2);
	writel(0x3f300000, &ccm->CCGR3);
	writel(0xff00f300, &ccm->CCGR4);
	writel(0x0f0000c3, &ccm->CCGR5);
	writel(0x000003cc, &ccm->CCGR6);
}

static void ddr_init(int *table, int size)
{
	int i;

	for (i = 0; i < size / 2 ; i++)
		writel(table[2 * i + 1], table[2 * i]);
}

void board_init_f(ulong dummy)
{
	ccgr_init();

	/* setup AIPS and disable watchdog */
	arch_cpu_init();

	gpr_init();

	/* iomux */
	SETUP_IOMUX_PADS(uart3_pads);

	/* setup GP timer */
	timer_init();

	/* UART clocks enabled and gd valid - init serial console */
	preloader_console_init();

	/* DDR initialization */
	ddr_init(mx6dl_dcd_table, ARRAY_SIZE(mx6dl_dcd_table));
}
