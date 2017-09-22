/*
 * cmd_nand_bootupdate.c - write U-Boot to MXS NAND to make it bootable
 *
 * Copyright (c) 2016 Sergey Kubushyn <ksi@koi8.net>
 *
 * Most of the source shamelesly stolen from barebox.
 *
 * Here is the original copyright:
 *
 *=== Cut ===
 * Copyright (C) 2014 Sascha Hauer, Pengutronix
 *=== Cut ===
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 */

#include <common.h>
#include <linux/sizes.h>
#include <linux/mtd/mtd.h>
#include <linux/compat.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <jffs2/jffs2.h>
#include <nand.h>
#include <errno.h>
#include <asm/imx-common/mxs_nand.h>
#include <asm/imx-common/imximage.cfg>


#ifndef CONFIG_CMD_MTDPARTS
#error "CONFIG_CMD_MTDPARTS is not set, mtd partition support missing"
#endif

static const char *uboot_tgt = "nand0,0";

/* partition handling routines */
int mtdparts_init(void);
int id_parse(const char *id, const char **ret_id, u8 *dev_type, u8 *dev_num);
int find_dev_and_part(const char *id, struct mtd_device **dev,
		      u8 *part_num, struct part_info **part);

struct dbbt_block {
	uint32_t Checksum;	/* reserved on i.MX6 */
	uint32_t FingerPrint;
	uint32_t Version;
	uint32_t numberBB;	/* reserved on i.MX6 */
	uint32_t DBBTNumOfPages;
};

struct fcb_block {
	uint32_t Checksum;		/* First fingerprint in first byte */
	uint32_t FingerPrint;		/* 2nd fingerprint at byte 4 */
	uint32_t Version;		/* 3rd fingerprint at byte 8 */
	uint8_t DataSetup;
	uint8_t DataHold;
	uint8_t AddressSetup;
	uint8_t DSAMPLE_TIME;
	/* These are for application use only and not for ROM. */
	uint8_t NandTimingState;
	uint8_t REA;
	uint8_t RLOH;
	uint8_t RHOH;
	uint32_t PageDataSize;		/* 2048 for 2K pages, 4096 for 4K pages */
	uint32_t TotalPageSize;		/* 2112 for 2K pages, 4314 for 4K pages */
	uint32_t SectorsPerBlock;	/* Number of 2K sections per block */
	uint32_t NumberOfNANDs;		/* Total Number of NANDs - not used by ROM */
	uint32_t TotalInternalDie;	/* Number of separate chips in this NAND */
	uint32_t CellType;		/* MLC or SLC */
	uint32_t EccBlockNEccType;	/* Type of ECC, can be one of BCH-0-20 */
	uint32_t EccBlock0Size;		/* Number of bytes for Block0 - BCH */
	uint32_t EccBlockNSize;		/* Block size in bytes for all blocks other than Block0 - BCH */
	uint32_t EccBlock0EccType;	/* Ecc level for Block 0 - BCH */
	uint32_t MetadataBytes;		/* Metadata size - BCH */
	uint32_t NumEccBlocksPerPage;	/* Number of blocks per page for ROM use - BCH */
	uint32_t EccBlockNEccLevelSDK;	/* Type of ECC, can be one of BCH-0-20 */
	uint32_t EccBlock0SizeSDK;	/* Number of bytes for Block0 - BCH */
	uint32_t EccBlockNSizeSDK;	/* Block size in bytes for all blocks other than Block0 - BCH */
	uint32_t EccBlock0EccLevelSDK;	/* Ecc level for Block 0 - BCH */
	uint32_t NumEccBlocksPerPageSDK;/* Number of blocks per page for SDK use - BCH */
	uint32_t MetadataBytesSDK;	/* Metadata size - BCH */
	uint32_t EraseThreshold;	/* To set into BCH_MODE register */
	uint32_t BootPatch;		/* 0 for normal boot and 1 to load patch starting next to FCB */
	uint32_t PatchSectors;		/* Size of patch in sectors */
	uint32_t Firmware1_startingPage;/* Firmware image starts on this sector */
	uint32_t Firmware2_startingPage;/* Secondary FW Image starting Sector */
	uint32_t PagesInFirmware1;	/* Number of sectors in firmware image */
	uint32_t PagesInFirmware2;	/* Number of sector in secondary FW image */
	uint32_t DBBTSearchAreaStartAddress; /* Page address where dbbt search area begins */
	uint32_t BadBlockMarkerByte;	/* Byte in page data that have manufacturer marked bad block marker, */
					/* this will be swapped with metadata[0] to complete page data. */
	uint32_t BadBlockMarkerStartBit;/* For BCH ECC sizes other than 8 and 16 the bad block marker does not */
					/* start at 0th bit of BadBlockMarkerByte. This field is used to get to */
					/* the start bit of bad block marker byte with in BadBlockMarkerByte */
	uint32_t BBMarkerPhysicalOffset;/* FCB value that gives byte offset for bad block marker on physical NAND page */
	uint32_t BCHType;

	uint32_t TMTiming2_ReadLatency;
	uint32_t TMTiming2_PreambleDelay;
	uint32_t TMTiming2_CEDelay;
	uint32_t TMTiming2_PostambleDelay;
	uint32_t TMTiming2_CmdAddPause;
	uint32_t TMTiming2_DataPause;
	uint32_t TMSpeed;
	uint32_t TMTiming1_BusyTimeout;

	uint32_t DISBBM;	/* the flag to enable (1)/disable(0) bi swap */
	uint32_t BBMarkerPhysicalOffsetInSpareData; /* The swap position of main area in spare area */
};

struct fw_write_data {
	int	fw1_blk;
	int	fw2_blk;
	int	part_blks;
	void	*buf;
	size_t	len;
};

#define BF_VAL(v, bf)	(((v) & bf##_MASK) >> bf##_OFFSET)
#define GETBIT(v,n)	(((v) >> (n)) & 0x1)


static uint8_t calculate_parity_13_8(uint8_t d)
{
	uint8_t	p = 0;

	p |= (GETBIT(d, 6) ^ GETBIT(d, 5) ^ GETBIT(d, 3) ^ GETBIT(d, 2))		 << 0;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 5) ^ GETBIT(d, 4) ^ GETBIT(d, 2) ^ GETBIT(d, 1)) << 1;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 6) ^ GETBIT(d, 5) ^ GETBIT(d, 1) ^ GETBIT(d, 0)) << 2;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 4) ^ GETBIT(d, 3) ^ GETBIT(d, 0))		 << 3;
	p |= (GETBIT(d, 6) ^ GETBIT(d, 4) ^ GETBIT(d, 3) ^ GETBIT(d, 2) ^ GETBIT(d, 1) ^ GETBIT(d, 0)) << 4;
	return p;
}

static void encode_hamming_13_8(void *_src, void *_ecc, size_t size)
{
	int	i;
	uint8_t	*src = _src;
	uint8_t	*ecc = _ecc;

	for (i = 0; i < size; i++)
		ecc[i] = calculate_parity_13_8(src[i]);
}

static uint32_t calc_chksum(void *buf, size_t size)
{
	u32	chksum = 0;
	u8	*bp = buf;
	size_t	i;

	for (i = 0; i < size; i++)
		chksum += bp[i];

	return ~chksum;
}


static ssize_t raw_write_page(struct mtd_info *mtd, void *buf, loff_t offset)
{
	struct mtd_oob_ops ops;
	ssize_t ret;

	ops.mode = MTD_OPS_RAW;
	ops.ooboffs = 0;
	ops.datbuf = buf;
	ops.len = mtd->writesize;
	ops.oobbuf = buf + mtd->writesize;
	ops.ooblen = mtd->oobsize;
	ret = mtd_write_oob(mtd, offset, &ops);

        return ret;
}

static int fcb_create(struct fcb_block *fcb, struct mtd_info *mtd)
{
	fcb->FingerPrint = 0x20424346;
	fcb->Version = 0x01000000;
	fcb->PageDataSize = mtd->writesize;
	fcb->TotalPageSize = mtd->writesize + mtd->oobsize;
	fcb->SectorsPerBlock = mtd->erasesize / mtd->writesize;

	/* Divide ECC strength by two and save the value into FCB structure. */
	fcb->EccBlock0EccType =
		mxs_nand_get_ecc_strength(mtd->writesize, mtd->oobsize) >> 1;

	fcb->EccBlockNEccType = fcb->EccBlock0EccType;

	/* Also hardcoded in kobs-ng */
	fcb->EccBlock0Size = 0x00000200;
	fcb->EccBlockNSize = 0x00000200;
	fcb->DataSetup = 80;
	fcb->DataHold = 60;
	fcb->AddressSetup = 25;
	fcb->DSAMPLE_TIME = 6;
	fcb->MetadataBytes = 10;

	/* DBBT search area starts at second page on first block */
	fcb->DBBTSearchAreaStartAddress = 1;

	fcb->BadBlockMarkerByte = mxs_nand_mark_byte_offset(mtd);
	fcb->BadBlockMarkerStartBit = mxs_nand_mark_bit_offset(mtd);

	fcb->BBMarkerPhysicalOffset = mtd->writesize;

	fcb->NumEccBlocksPerPage = mtd->writesize / fcb->EccBlock0Size - 1;

	fcb->Checksum = calc_chksum((void *)fcb + 4, sizeof(*fcb) - 4);

	return 0;
}

/* Erase entire U-Boot partition */
static int imx_nand_uboot_erase(struct mtd_info *mtd, struct part_info *part)
{
	uint64_t		offset = 0;
	struct erase_info	erase;
	int			len = part->size;
	int			ret;

	while (len > 0) {
		pr_debug("erasing at 0x%08llx\n", offset);
		if (mtd_block_isbad(mtd, offset)) {
			pr_debug("erase skip block @ 0x%08llx\n", offset);
			offset += mtd->erasesize;
			continue;
		}

		memset(&erase, 0, sizeof(erase));
		erase.addr = offset;
		erase.len = mtd->erasesize;

		ret = mtd_erase(mtd, &erase);
		if (ret)
			return ret;

		offset += mtd->erasesize;
		len -= mtd->erasesize;
	}

	return 0;
}

/*
 * Write the U-Boot proper (2 copies) to where it belongs.
 * This is U-Boot binary image itself, no FCB/DBBT here yet.
 */
static int imx_nand_uboot_write_fw(struct mtd_info *mtd, struct fw_write_data *fw)
{
	uint64_t	offset;
	int		ret;
	int		blk;
	size_t		dummy;
	size_t		bytes_left;
	int		chunk;
	void		*p;

	bytes_left = fw->len;
	p = fw->buf;
	blk = fw->fw1_blk;
	offset = blk * mtd->erasesize;

	while (bytes_left > 0) {
		chunk = min(bytes_left, mtd->erasesize);

		pr_debug("fw1: writing %p at 0x%08llx, left 0x%08x\n",
				p, offset, bytes_left);

		if (mtd_block_isbad(mtd, offset)) {
			pr_debug("fw1: write skip block @ 0x%08llx\n", offset);
			offset += mtd->erasesize;
			blk++;
			continue;
		}

		if (blk >= fw->fw2_blk)
			return -ENOSPC;

		ret = mtd_write(mtd, offset, chunk, &dummy, p);
		if (ret)
			return ret;

		offset += chunk;
		bytes_left -= chunk;
		p += chunk;
		blk++;
	}

	bytes_left = fw->len;
	p = fw->buf;
	blk = fw->fw2_blk;
	offset = blk * mtd->erasesize;

	while (bytes_left > 0) {
		chunk = min(bytes_left, mtd->erasesize);

		pr_debug("fw2: writing %p at 0x%08llx, left 0x%08x\n",
				p, offset, bytes_left);

		if (mtd_block_isbad(mtd, offset)) {
			pr_debug("fw2: write skip block @ 0x%08llx\n", offset);
			offset += mtd->erasesize;
			blk++;
			continue;
		}

		if (blk >= fw->part_blks)
			return -ENOSPC;

		ret = mtd_write(mtd, offset, chunk, &dummy, p);
		if (ret)
			return ret;

		offset += chunk;
		bytes_left -= chunk;
		p += chunk;
		blk++;
	}

	return 0;
}

static int dbbt_data_create(struct mtd_info *mtd, void *buf, int num_blocks)
{
	int		n;
	int		n_bad_blocks = 0;
	uint32_t	*bb = buf + 0x8;
	uint32_t	*n_bad_blocksp = buf + 0x4;

	for (n = 0; n < num_blocks; n++) {
		loff_t offset = n * mtd->erasesize;
		if (mtd_block_isbad(mtd, offset)) {
			n_bad_blocks++;
			*bb = n;
			bb++;
		}
	}

	*n_bad_blocksp = n_bad_blocks;

	return n_bad_blocks;
}

/*
 * This is where it is all done. Takes pointer to a U-Boot image in
 * RAM and image size, creates FCB/DBBT and writes everything where
 * it belongs into NAND. Image must be an IMX image built for NAND.
 */
static int imx_nand_uboot_update(const void *img, size_t len)
{
	int 			i, ret;

	size_t			dummy;
	loff_t			offset = 0;

	void 			*fcb_raw_page;
	void			*dbbt_page;
	void			*dbbt_data_page;
	void			*ecc;

	uint32_t 		num_blocks_fcb_dbbt;
	uint32_t		num_blocks_fw;

	struct mtd_info		*mtd;
	struct fcb_block	*fcb;
	struct dbbt_block	*dbbt;

	struct mtd_device	*dev;
	struct part_info	*part;
	u8			pnum;

	struct fw_write_data	fw;

	if ((mtdparts_init() == 0) &&
			(find_dev_and_part(uboot_tgt, &dev, &pnum, &part) == 0)) {
		if (dev->id->type != MTD_DEV_TYPE_NAND) {
			puts("Not a NAND device\n");
			return -ENODEV;
		}
	}

	nand_curr_device = dev->id->num;

#ifdef CONFIG_SYS_NAND_SELECT_DEVICE
	board_nand_select_device(nand_info[nand_curr_device].priv, nand_curr_device);
#endif

	/* Get a pointer to mtd_info for selected device */

	mtd = get_mtd_device_nm("nand0");	/* We always boot off of nand0 */

	if (IS_ERR(mtd)) {
		/* Should not happen */
		puts("No nand0 device...\n");
		return -ENODEV;
	}

	put_mtd_device(mtd);

	/* Quick and dirty check if we have 2Mbytes of good blocks in nand0,0 */

	i = 0;
	offset = 0;	/* It is the first partition so it starts at block 0 */

	while (offset < part->size) {
		if (!mtd_block_isbad(mtd, offset)) {
			i += mtd->erasesize;
		}
		offset += mtd->erasesize;
	}

	if (i < SZ_2M) {
		puts("Partition too small for U-Boot!\n");
		return -EINVAL;
	}

	/*
	 * We will use 4 first blocks for FCB/DBBT copies.
	 * The rest of partition is split in half and used
	 * for two U-Boot copies. We don't care if those
	 * start on good or bad block - bad blocks will be
	 * skipped on write, ROM boot code will also skip
	 * bad blocks on bootup when loading U-Boot image.
	 */

	fw.part_blks = part->size / mtd->erasesize;
	num_blocks_fcb_dbbt = 4;
	num_blocks_fw = (fw.part_blks - num_blocks_fcb_dbbt) / 2;
	fw.fw1_blk = num_blocks_fcb_dbbt;
	fw.fw2_blk = fw.fw1_blk + num_blocks_fw;

	/* OK, now create FCB structure for bootROM NAND boot */

	fcb_raw_page = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);

	fcb = fcb_raw_page + 12;
	ecc = fcb_raw_page + 512 + 12;

	dbbt_page = kzalloc(mtd->writesize, GFP_KERNEL);
	dbbt_data_page = kzalloc(mtd->writesize, GFP_KERNEL);
	dbbt = dbbt_page;

	/*
	 * Write one additional page to make the ROM happy.
	 * Maybe the PagesInFirmwarex fields are really the
	 * number of pages - 1. kobs-ng does the same.
	 */

	fw.len = ALIGN(len + FLASH_OFFSET_STANDARD + mtd->writesize, mtd->writesize);
	fw.buf = kzalloc(fw.len, GFP_KERNEL);
	memcpy(fw.buf + FLASH_OFFSET_STANDARD, img, len);

	/* Erase entire partition */
	ret = imx_nand_uboot_erase(mtd, part);
	if (ret)
		goto out;

	/*
	 * Now write 2 copies of the U-Boot proper to where they belong.
	 * Headers (FCB, DBBT) will be generated and written after that.
	 */
	ret = imx_nand_uboot_write_fw(mtd, &fw);
	if (ret < 0)
		goto out;

	/*
	 * Create FCB, calculate ECC (we don't/can't use hardware ECC
	 * here so we do it ourselves and then write _RAW_ pages.
	 */

	fcb->Firmware1_startingPage = fw.fw1_blk * mtd->erasesize / mtd->writesize;
	fcb->Firmware2_startingPage = fw.fw2_blk * mtd->erasesize / mtd->writesize;
	fcb->PagesInFirmware1 =
		ALIGN(len + FLASH_OFFSET_STANDARD, mtd->writesize) / mtd->writesize;
	fcb->PagesInFirmware2 = fcb->PagesInFirmware1;

	fcb_create(fcb, mtd);
	encode_hamming_13_8(fcb, ecc, 512);

	/*
	 * Set the first and second byte of OOB data to 0xFF, not 0x00. These
	 * bytes are used as the Manufacturers Bad Block Marker (MBBM). Since
	 * the FCB is mostly written to the first page in a block, a scan for
	 * factory bad blocks will detect these blocks as bad, e.g. when
	 * function nand_scan_bbt() is executed to build a new bad block table.
	 * We will _NOT_ mark a bad block as good -- we skip the bad blocks.
	 */
	memset(fcb_raw_page + mtd->writesize, 0xff, 2);

	/* Now create DBBT */
	dbbt->Checksum = 0;
	dbbt->FingerPrint = 0x54424244;
	dbbt->Version = 0x01000000;

	if ((ret = dbbt_data_create(mtd, dbbt_data_page, fw.part_blks)) < 0)
		goto out;

	if (ret > 0)
		dbbt->DBBTNumOfPages = 1;

	offset = 0;

	if (mtd_block_isbad(mtd, offset)) {
		puts("Block 0 is bad, NAND unusable\n");
		ret = -EIO;
		goto out;
	}

	/*
	 * Write FCB/DBBT to first 4 blocks. Skip bad blocks if any.
	 * Less than 4 copies will be written if there were BBs !!!
	 */

	for (i = 0; i < 4; i++) {

		if (mtd_block_isbad(mtd, offset)) {
			pr_err("Block %d is bad, skipped\n", i);
			continue;
		}


		ret = raw_write_page(mtd, fcb_raw_page, mtd->erasesize * i);
		if (ret)
			goto out;

		ret = mtd_write(mtd, mtd->erasesize * i + mtd->writesize,
				mtd->writesize, &dummy, dbbt_page);
		if (ret)
			goto out;

		/* DBBTNumOfPages == 0 if no bad blocks */
		if (dbbt->DBBTNumOfPages > 0) {
			ret = mtd_write(mtd, mtd->erasesize * i + mtd->writesize * 5,
					mtd->writesize, &dummy, dbbt_data_page);
			if (ret)
				goto out;
		}
	}

out:
	kfree(dbbt_page);
	kfree(dbbt_data_page);
	kfree(fcb_raw_page);
	kfree(fw.buf);

	return ret;
}


int do_nand_bootupdate(ulong addr, size_t len)
{
	/* KSI: Unlock NAND first if it is locked... */

	return imx_nand_uboot_update((const void *)addr, len);
}
