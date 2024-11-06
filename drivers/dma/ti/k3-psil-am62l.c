// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2024 Texas Instruments Incorporated - https://www.ti.com
 */

#include <linux/kernel.h>

#include "k3-psil-priv.h"

#define PSIL_PDMA_XY_PKT(x, ch, irq)					\
	{							\
		.thread_id = x,					\
		.ep_config = {					\
			.ep_type = PSIL_EP_PDMA_XY,		\
			.mapped_channel_id = ch,		\
			.pkt_mode = 0,				\
			.irq_idx = irq,		\
			.default_flow_id = -1	\
		},						\
	}

#define PSIL_ETHERNET(x, ch, flow_base, flow_cnt)		\
	{							\
		.thread_id = x,					\
		.ep_config = {					\
			.ep_type = PSIL_EP_NATIVE,		\
			.pkt_mode = 1,				\
			.needs_epib = 1,			\
			.psd_size = 16,				\
			.mapped_channel_id = ch,		\
			.flow_start = flow_base,		\
			.flow_num = flow_cnt,			\
			.default_flow_id = flow_base,		\
		},						\
	}

#define PSIL_SAUL(x, ch, flow_base, flow_cnt, default_flow, tx)	\
	{							\
		.thread_id = x,					\
		.ep_config = {					\
			.ep_type = PSIL_EP_NATIVE,		\
			.pkt_mode = 1,				\
			.needs_epib = 1,			\
			.psd_size = 64,				\
			.mapped_channel_id = ch,		\
			.flow_start = flow_base,		\
			.flow_num = flow_cnt,			\
			.default_flow_id = default_flow,	\
			.notdpkt = tx,				\
		},						\
	}

#define PSIL_PDMA_MCASP(x, ch, irq)				\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_PDMA_XY,	\
			.pdma_acc32 = 1,		\
			.pdma_burst = 1,		\
			.mapped_channel_id = ch,	\
			.irq_idx = irq,		\
		},					\
	}

#define PSIL_CSI2RX(x)					\
	{						\
		.thread_id = x,				\
		.ep_config = {				\
			.ep_type = PSIL_EP_NATIVE,	\
		},					\
	}

/* PSI-L source thread IDs, used for RX (DMA_DEV_TO_MEM) */
static struct psil_ep am62l_src_ep_map[] = {
	/* PDMA_MAIN1 - UART0-6 */
	PSIL_PDMA_XY_PKT(0x4400, 0, 0),
	PSIL_PDMA_XY_PKT(0x4401, 2, 1),
	PSIL_PDMA_XY_PKT(0x4402, 4, 2),
	PSIL_PDMA_XY_PKT(0x4403, 6, 3),
	PSIL_PDMA_XY_PKT(0x4404, 8, 4),
	PSIL_PDMA_XY_PKT(0x4405, 10, 5),
	PSIL_PDMA_XY_PKT(0x4406, 12, 6),
	/* PDMA_MAIN0 - SPI0 - CH0-3 */
	PSIL_PDMA_XY_PKT(0x4300, 16, 7),
	PSIL_PDMA_XY_PKT(0x4300, 18, 7),
	PSIL_PDMA_XY_PKT(0x4300, 20, 7),
	PSIL_PDMA_XY_PKT(0x4300, 22, 7),
	/* PDMA_MAIN0 - SPI1 - CH0-3 */
	PSIL_PDMA_XY_PKT(0x4301, 24, 8),
	PSIL_PDMA_XY_PKT(0x4301, 26, 8),
	PSIL_PDMA_XY_PKT(0x4301, 28, 8),
	PSIL_PDMA_XY_PKT(0x4301, 30, 8),
	/* PDMA_MAIN0 - SPI2 - CH0-3 */
	PSIL_PDMA_XY_PKT(0x4302, 32, 9),
	PSIL_PDMA_XY_PKT(0x4302, 34, 9),
	PSIL_PDMA_XY_PKT(0x4302, 36, 9),
	PSIL_PDMA_XY_PKT(0x4302, 38, 9),
	/* PDMA_MAIN0 - SPI3 - CH0-3 */
	PSIL_PDMA_XY_PKT(0x4303, 40, 10),
	PSIL_PDMA_XY_PKT(0x4303, 42, 10),
	PSIL_PDMA_XY_PKT(0x4303, 44, 10),
	PSIL_PDMA_XY_PKT(0x4303, 46, 10),
	/* PDMA_MAIN2 - MCASP0-2 */
	PSIL_PDMA_MCASP(0x4500, 48, 11),
	PSIL_PDMA_MCASP(0x4501, 50, 12),
	PSIL_PDMA_MCASP(0x4502, 52, 13),
	/* PDMA_MAIN0 - AES */
	PSIL_PDMA_XY_PKT(0x4700, 65, 14),
	/* PDMA_MAIN0 - ADC */
	PSIL_PDMA_XY_PKT(0x4503, 80, 15),
	PSIL_PDMA_XY_PKT(0x4504, 81, 15),
	PSIL_ETHERNET(0x4600, 96, 96, 16),
};

/* PSI-L destination thread IDs, used for TX (DMA_MEM_TO_DEV) */
static struct psil_ep am62l_dst_ep_map[] = {
	/* PDMA_MAIN1 - UART0-6 */
	PSIL_PDMA_XY_PKT(0xC400, 1, 16),
	PSIL_PDMA_XY_PKT(0xC401, 3, 17),
	PSIL_PDMA_XY_PKT(0xC402, 5, 18),
	PSIL_PDMA_XY_PKT(0xC403, 7, 19),
	PSIL_PDMA_XY_PKT(0xC404, 9, 20),
	PSIL_PDMA_XY_PKT(0xC405, 11, 21),
	PSIL_PDMA_XY_PKT(0xC406, 13, 22),
	/* PDMA_MAIN0 - SPI0 - CH0-3 */
	PSIL_PDMA_XY_PKT(0xC300, 17, 23),
	PSIL_PDMA_XY_PKT(0xC300, 19, 23),
	PSIL_PDMA_XY_PKT(0xC300, 21, 23),
	PSIL_PDMA_XY_PKT(0xC300, 23, 23),
	/* PDMA_MAIN0 - SPI1 - CH0-3 */
	PSIL_PDMA_XY_PKT(0xC301, 25, 24),
	PSIL_PDMA_XY_PKT(0xC301, 27, 24),
	PSIL_PDMA_XY_PKT(0xC301, 29, 24),
	PSIL_PDMA_XY_PKT(0xC301, 31, 24),
	/* PDMA_MAIN0 - SPI2 - CH0-3 */
	PSIL_PDMA_XY_PKT(0xC302, 33, 25),
	PSIL_PDMA_XY_PKT(0xC302, 35, 25),
	PSIL_PDMA_XY_PKT(0xC302, 37, 25),
	PSIL_PDMA_XY_PKT(0xC302, 39, 25),
	/* PDMA_MAIN0 - SPI3 - CH0-3 */
	PSIL_PDMA_XY_PKT(0xC303, 41, 26),
	PSIL_PDMA_XY_PKT(0xC303, 43, 26),
	PSIL_PDMA_XY_PKT(0xC303, 45, 26),
	PSIL_PDMA_XY_PKT(0xC303, 47, 26),
	/* PDMA_MAIN2 - MCASP0-2 */
	PSIL_PDMA_MCASP(0xC500, 49, 27),
	PSIL_PDMA_MCASP(0xC501, 51, 28),
	PSIL_PDMA_MCASP(0xC502, 53, 29),
	/* PDMA_MAIN0 - SHA */
	PSIL_PDMA_XY_PKT(0xC700, 64, 30),
	/* PDMA_MAIN0 - AES */
	PSIL_PDMA_XY_PKT(0x4701, 66, 31),
	/* PDMA_MAIN0 - CRC32 - CH0-1 */
	PSIL_PDMA_XY_PKT(0xC702, 67, 32),
	PSIL_PDMA_XY_PKT(0xC702, 68, 32),
	/* CPSW3G */
	PSIL_ETHERNET(0xc600, 64, 64, 2),
	PSIL_ETHERNET(0xc601, 66, 66, 2),
	PSIL_ETHERNET(0xc602, 68, 68, 2),
	PSIL_ETHERNET(0xc603, 70, 70, 2),
	PSIL_ETHERNET(0xc604, 72, 72, 2),
	PSIL_ETHERNET(0xc605, 74, 74, 2),
	PSIL_ETHERNET(0xc606, 76, 76, 2),
	PSIL_ETHERNET(0xc607, 78, 78, 2),
};

struct psil_ep_map am62l_ep_map = {
	.name = "am62l",
	.src = am62l_src_ep_map,
	.src_count = ARRAY_SIZE(am62l_src_ep_map),
	.dst = am62l_dst_ep_map,
	.dst_count = ARRAY_SIZE(am62l_dst_ep_map),
};
