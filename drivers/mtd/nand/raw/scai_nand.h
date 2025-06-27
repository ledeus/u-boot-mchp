#include <common.h>

#ifndef _SCAI_NAND_H
#define _SCAI_NAND_H

/* APB base address */
static const u32 apb_base_address = 0x70000000UL;

/* Base addresses for QSPI NAND (M0–M7) */
static const u32 scai_nand_base_addresses[] = {
    apb_base_address + 0x0400,  /* M0 */
    apb_base_address + 0x0410,  /* M1 */
    apb_base_address + 0x0420,  /* M2 */
    apb_base_address + 0x0430,  /* M3 */
    apb_base_address + 0x0500,  /* M4 */
    apb_base_address + 0x0510,  /* M5 */
    apb_base_address + 0x0520,  /* M6 */
    apb_base_address + 0x0530   /* M7 */
};

/* Register offsets in FPGA */
static const u32 q_wr_ctrl1_offset   = 0x00;  /* Control register (including CE) */
static const u32 q_wr_data_offset    = 0x04;  /* Data for transmission */
static const u32 q_rd_data_offset    = 0x08;  /* Data for reading */
static const u32 q_rd_status_offset  = 0x0C; /* Operation status */

/* Control bits */
static const u32 q_ctrl1_enable      = 0x00000001;
static const u32 q_ctrl1_activate_ce = 0x00000002;
static const u32 q_ctrl1_start_op    = 0x00000800;
static const u32 q_ctrl1_set_x4nx1   = 0x00000020;  /* 1: QSPI, 0: SPI */
static const u32 q_idle              = 0x00000001;  /* IDLE bit in status */

/* Parameters for Micron MT29F8G01ADBFD12-AAT (8 chips) */
static const u32 page_size           = 4096;   /* 4KB */
static const u32 oob_size            = 256;    /* 256 bytes OOB */
static const u32 block_size          = 256 * 1024;  /* 256KB */
static const u64 chip_size           = 1ULL * 1024 * 1024 * 1024;  /* 1GB per chip */
static const u64 total_size          = 8ULL * chip_size;  /* 8GB total */
static const u32 num_chips           = 8;      /* Number of chips */

/* Private structure for chip context */
struct scai_nand_priv {
    u32 base_addr;      /* APB base address for chip */
    void __iomem *regs; /* Mapped register base */
};

#endif /* _SCAI_NAND_H */