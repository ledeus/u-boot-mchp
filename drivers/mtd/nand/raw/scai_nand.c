#include <common.h>
#include <dm.h>
#include <nand.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>

#inclue "scai_nand.h"

/* Get chip index from address */
static inline int get_chip_index(loff_t addr)
{
    return (addr / chip_size);  /* Each chip is 1GB */
}

static void scai_nand_select_chip(struct scai_nand_priv *priv, int chip_idx, int activate)
{
    u32 ctrl = readl(priv[chip_idx].regs + q_wr_ctrl1_offset);
    
    if (activate)
        ctrl |= q_ctrl1_activate_ce;
    else
        ctrl &= ~q_ctrl1_activate_ce;
        
    writel(ctrl, priv[chip_idx].regs + q_wr_ctrl1_offset);
}

static void scai_nand_cmd_write(struct scai_nand_priv *priv, int chip_idx, u8 cmd, loff_t addr)
{
    u32 ctrl = q_ctrl1_enable | q_ctrl1_start_op;
    
    /* Configure for standard SPI (not QSPI) */
    ctrl &= ~q_ctrl1_set_x4nx1;
    
    /* Send command */
    writel(cmd, priv[chip_idx].regs + q_wr_data_offset);
    writel(ctrl, priv[chip_idx].regs + q_wr_ctrl1_offset);
    
    /* Wait for operation to complete */
    while (!(readl(priv[chip_idx].regs + q_rd_status_offset) & q_idle))
        udelay(1);
        
    /* Send address (24 bits for Micron MT29F) */
    writel((u32)(addr >> 8), priv[chip_idx].regs + q_wr_data_offset);
    writel(ctrl, priv[chip_idx].regs + q_wr_ctrl1_offset);
    
    /* Wait for operation to complete */
    while (!(readl(priv[chip_idx].regs + q_rd_status_offset) & q_idle))
        udelay(1);
}

static int scai_nand_read_buf(struct mtd_info *mtd, struct scai_nand_priv *priv, int chip_idx, u_char *buf, int len)
{
    u32 ctrl = q_ctrl1_enable | q_ctrl1_start_op;
    
    /* Configure for standard SPI */
    ctrl &= ~q_ctrl1_set_x4nx1;
    
    /* Read data */
    for (int i = 0; i < len; i++) {
        writel(ctrl, priv[chip_idx].regs + q_wr_ctrl1_offset);
        while (!(readl(priv[chip_idx].regs + q_rd_status_offset) & q_idle))
            udelay(1);
        buf[i] = readb(priv[chip_idx].regs + q_rd_data_offset);
    }
    
    return 0;
}

static int scai_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
                               u_char *buf, int oob_required, int page)
{
    struct scai_nand_priv *priv = mtd->priv;
    loff_t from = (loff_t)page * page_size;
    int chip_idx = get_chip_index(from);
    loff_t chip_offset = from % chip_size;
    
    /* Select chip */
    scai_nand_select_chip(priv, chip_idx, 1);
    
    /* Send page read to cache command */
    scai_nand_cmd_write(priv, chip_idx, NAND_READ_PAGE, chip_offset);
    
    /* Read data from cache */
    scai_nand_cmd_write(priv, chip_idx, NAND_READ_CACHE, 0);
    
    /* Read main buffer */
    scai_nand_read_buf(mtd, priv, chip_idx, buf, page_size);
    
    /* Read OOB if required */
    if (oob_required && chip->oob_poi)
        scai_nand_read_buf(mtd, priv, chip_idx, chip->oob_poi, oob_size);
    
    /* Deselect chip */
    scai_nand_select_chip(priv, chip_idx, 0);
    
    return 0;
}

static int scai_nand_init(struct nand_chip *chip)
{
    chip->options |= NAND_NO_SUBPAGE_WRITE;
    chip->ecc.mode = NAND_ECC_NONE;  /* ECC disabled */
    
    chip->read_page = scai_nand_read_page;
    
    return 0;
}

static int scai_nand_probe(struct udevice *dev)
{
    struct nand_chip *chip = dev_get_priv(dev);
    struct mtd_info *mtd = nand_to_mtd(chip);
    struct scai_nand_priv *priv;
    
    /* Allocate memory for private data (8 chips) */
    priv = devm_kzalloc(dev, num_chips * sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;
        
    /* Initialize chip addresses and map registers */
    for (int i = 0; i < num_chips; i++) {
        priv[i].base_addr = scai_nand_base_addresses[i];
        priv[i].regs = ioremap(priv[i].base_addr, 0x100);
        if (!priv[i].regs) {
            /* Clean up previous mappings on error */
            for (int j = 0; j < i; j++)
                iounmap(priv[j].regs);
            return -ENOMEM;
        }
    }
        
    mtd->priv = priv;
    
    /* Initialize NAND */
    scai_nand_init(chip);
    
    /* Configure MTD parameters */
    mtd->size = total_size;
    mtd->erasesize = block_size;
    mtd->writesize = page_size;
    mtd->oobsize = oob_size;
    
    /* Register NAND device */
    return nand_register(0, mtd);
}

static void scai_nand_remove(struct udevice *dev)
{
    struct nand_chip *chip = dev_get_priv(dev);
    struct mtd_info *mtd = nand_to_mtd(chip);
    struct scai_nand_priv *priv = mtd->priv;
    
    /* Unmap registers for all chips */
    for (int i = 0; i < num_chips; i++)
        iounmap(priv[i].regs);
}

static const struct udevice_id scai_nand_ids[] = {
    { .compatible = "microchip,scai-nand" },
    { }
};

U_BOOT_DRIVER(scai_nand) = {
    .name = "scai-nand",
    .id = UCLASS_MTD,
    .of_match = scai_nand_ids,
    .probe = scai_nand_probe,
    .remove = scai_nand_remove,
    .priv_auto = sizeof(struct nand_chip),
};
