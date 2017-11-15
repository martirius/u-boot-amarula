/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:  GPL-2.0+
 */

#ifndef __IMX_RDC_H__
#define __IMX_RDC_H__

#if defined(CONFIG_IMX6SX)
#include "mx6sx_rdc.h"
#else
#error "Please select cpu"
#endif /* CONFIG_IMX6SX */

#endif	/* __IMX_RDC_H__*/
