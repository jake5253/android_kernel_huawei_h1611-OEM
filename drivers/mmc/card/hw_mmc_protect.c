#include <linux/module.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/printk.h>
#include <linux/errno.h>
#include "queue.h"

#define PARTITION_NUM    2

struct protect_partition_info {
	char start_partition[32];
    char end_partition[32];
};

static struct protect_partition_info protect_partition_list[PARTITION_NUM] = {
	{{0},        "modem"},
	{"cust",        "system"},
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
							\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})
struct mmc_blk_data {
	spinlock_t  lock;
	struct gendisk  *disk;
	struct mmc_queue queue;
	struct list_head part;

	unsigned int    flags;
#define MMC_BLK_CMD23   (1 << 0)    /* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR  (1 << 1)    /* MMC Reliable write support */
#define MMC_BLK_PACKED_CMD  (1 << 2)    /* MMC packed command support */
 
	unsigned int    usage;
	unsigned int    read_only;
	unsigned int    part_type;
	unsigned int    name_idx;
	unsigned int    reset_done;
#define MMC_BLK_READ        BIT(0)
#define MMC_BLK_WRITE       BIT(1)
#define MMC_BLK_DISCARD     BIT(2)
#define MMC_BLK_SECDISCARD  BIT(3)
#define MMC_BLK_FLUSH       BIT(4)


	/*
	* Only set in main mmc_blk_data associated
	* with mmc_card with mmc_set_drvdata, and keeps
	* track of the current selected device partition.
	*/
	unsigned int    part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	struct device_attribute num_wr_reqs_to_start_packing;
	struct device_attribute bkops_check_threshold;
	struct device_attribute no_pack_for_random;
	int area_type;
};


/* get write protection block info */
#ifdef CONFIG_HUAWEI_KERNEL_DEBUG
static int hw_mmc_do_get_write_protection_info(struct gendisk *disk, struct hd_struct *start_part, struct hd_struct *end_part)
{
	struct mmc_blk_data *md;
	struct mmc_card *card;
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	void *data_buf;
	int len = 8;
	unsigned char buf[8], temp_char, wp_flag;
	unsigned int sector_start_addr, wp_group_size;
    unsigned int sector_end_addr, sector_size;
	char line_buf[128];
	int i, j, ret = 0;
	/* make sure this is a main partition*/
	md = mmc_blk_get(disk);
	if((!md) || (!(md->area_type & MMC_BLK_DATA_AREA_MAIN))) {
		ret = -EINVAL;
		return ret;
	}
	card = md->queue.card;
	if(IS_ERR(card)) {
		ret = PTR_ERR(card);
		return ret;
	}

	/* get system sector start address*/
    if ((!start_part) || (!end_part)) {
        pr_err("start_part or end_part is NULL\n");
        return -EINVAL;
    }

	sector_start_addr = (unsigned int)(start_part->start_sect);
    sector_end_addr = (unsigned int)(end_part->start_sect);
    sector_size = sector_end_addr - sector_start_addr;
	sector_size += (unsigned int)(end_part->nr_sects);

	wp_group_size = (512 * 1024) * card->ext_csd.raw_hc_erase_gap_size \
						* card->ext_csd.raw_hc_erase_grp_size / 512;
	sector_start_addr = sector_start_addr / wp_group_size * wp_group_size;
	pr_err("[INFO] %s: sector_start_addr = 0x%08x. wp_group_size = 0x%08x.\n", __func__, sector_start_addr, wp_group_size);
	data_buf = kzalloc(len, GFP_KERNEL);
	if(!data_buf) {
		pr_err("Malloc err at %d.\n", __LINE__);
		return -ENOMEM;
	}
	mrq.cmd = &cmd;
	mrq.data = &data;
	cmd.opcode = MMC_SEND_WRITE_PROT_TYPE;
	cmd.arg = sector_start_addr;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);
	mmc_rpm_hold(card->host, &card->dev);
	mmc_claim_host(card->host);
	mmc_set_data_timeout(&data, card);
	mmc_wait_for_req(card->host, &mrq);
	mmc_release_host(card->host);
	mmc_rpm_release(card->host, &card->dev);

	memcpy(buf, data_buf, len);

	for(i = 7; i >= 0; i--) {
		temp_char = buf[i];
		for(j = 0; j < 4; j++) {
			wp_flag = temp_char & 0x3;
			snprintf(line_buf, sizeof(line_buf),"[0x%08x~0x%08x] Write protection group is ",
						sector_start_addr, sector_start_addr + wp_group_size - 1);
			sector_start_addr += wp_group_size;
			temp_char = temp_char >> 2;
			switch(wp_flag) {
				case 0:
					strncat(line_buf, "disable", sizeof(line_buf));
					break;
				case 1:
					strncat(line_buf, "temporary write protection", sizeof(line_buf));
					break;
				case 2:
					strncat(line_buf, "power-on write protection", sizeof(line_buf));
					break;
				case 3:
					strncat(line_buf, "permanent write protection", sizeof(line_buf));
					break;
				default:
					break;
			}
			pr_err("%s: %s\n", mmc_hostname(card->host), line_buf);
		}
	}

	if(cmd.error) {
		ret = 1;
		pr_err("cmd.error=%d\n", cmd.error);
		goto out;
	}
	if(data.error) {
		ret = 1;
		pr_err("data.error=%d\n", data.error);
		goto out;
	}
out:
	kfree(data_buf);
	return ret;
}
#endif

/* physical write protect */
static int hw_mmc_do_set_write_protection(struct gendisk *disk, struct hd_struct *start_part, struct hd_struct *end_part)
{
	struct mmc_blk_data *md;
	struct mmc_card *card;
	struct mmc_command cmd = {0};
	unsigned int loop_count;
	unsigned int status;
	unsigned int sector_start_addr, sector_size, wp_group_size;
    unsigned int sector_end_addr;
	unsigned char tmp_wp, perm_wp, tmp = 0;
	int err = 0, i = 0;
	int retries = 3;
	u8 *ext_csd;

    if ((!start_part) || (!end_part)) {
        pr_err("start_part or end_part is NULL\n");
        return -EINVAL;
    }

	sector_start_addr = (unsigned int)(start_part->start_sect);
    sector_end_addr = (unsigned int)(end_part->start_sect);
    sector_size = sector_end_addr - sector_start_addr;
	sector_size += (unsigned int)(end_part->nr_sects);

	md = mmc_blk_get(disk);
	/* make sure this is a main partition*/
	if((!md) || (!(md->area_type & MMC_BLK_DATA_AREA_MAIN))) {
		err = -EINVAL;
		return err;
	}
	card = md->queue.card;
	if(IS_ERR(card)) {
		err = PTR_ERR(card);
	}

	/*  Calculating the loop count for sending SET_WRITE_PROTECT (CMD28) */
	wp_group_size = (512 * 1024) * card->ext_csd.raw_hc_erase_gap_size \
						* card->ext_csd.raw_hc_erase_grp_size / 512;

    
	if(sector_size % wp_group_size) {
		pr_err("%s: Write protected areas need to be aligned in accordance with wp_group_size.\n",
					mmc_hostname(card->host));
            //sector_size = (sector_size - wp_group_size - 1) & (~(wp_group_size - 1));
			return -EINVAL;
	} else {
		pr_err("%s: Addr is aligned with wp_gpoup_size.\n", mmc_hostname(card->host));
		loop_count = sector_size / wp_group_size;
	}
	if(sector_start_addr % wp_group_size) {
		pr_err("Addr sector_start is not aligned 0x%08x 0x%08x.\n", sector_start_addr, wp_group_size);
		sector_start_addr = sector_start_addr / wp_group_size * wp_group_size;
	} else {
		pr_err("Addr sector_start is aliged.\n");
	}

	ext_csd = kzalloc(512, GFP_KERNEL);
	if(!ext_csd) {
		pr_err("%s: unable to allocate buffer for ext_csd.\n", mmc_hostname(card->host));
		return -ENOMEM;
	}

	mmc_rpm_hold(card->host, &card->dev);
	mmc_claim_host(card->host);
	perm_wp = UNSTUFF_BITS(card->raw_csd, 13, 1);
	pr_info("tmp_wp = %d, perm_wp = %d.\n", tmp_wp, perm_wp);

	err = mmc_send_ext_csd(card, ext_csd);
	if(err) {
		pr_err("%s: err %d sending ext_csd.\n", mmc_hostname(card->host), err);
		goto out;
	}
	pr_err("INFO mmc_switch before, ext_csd.user_wp is 0x%x.\n", ext_csd[EXT_CSD_USER_WP]);
	#define EXT_CSD_USER_WP_B_PWR_WP_EN     (1 << 0)
	#define EXT_CSD_USER_WP_B_PERM_WP_EN    (1 << 2)
	#define EXT_CSD_USER_B_PERM_WP_DIS      (1 << 3)
	while(!(ext_csd[EXT_CSD_USER_WP] & EXT_CSD_USER_WP_B_PWR_WP_EN)) {
		if(retries-- == 0)
			goto out;
		/*
		* US_PERM_WP_EN   US_PWR_WP_EN   Type of protection set by SET_WRITE_PROT command
		*             0              0   Temporary
		*             0              1   Power-On
		*             1              0   Permanent
		*             1              1   Permanent
		*/

		/* enable power-on */
		tmp = ext_csd[EXT_CSD_USER_WP];
		tmp |= EXT_CSD_USER_WP_B_PWR_WP_EN;
		tmp &= ~EXT_CSD_USER_WP_B_PERM_WP_EN;
		tmp &= ~EXT_CSD_USER_B_PERM_WP_DIS;
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP, tmp, 0);
		if(err) {
			pr_err("%s: set write protect failed.\n", mmc_hostname(card->host));
		}
		err = mmc_send_ext_csd(card, ext_csd);
		if(err)
			goto out;
		pr_err("mmc_switch end, ext_csd.user_wp is 0x%x.\n", ext_csd[EXT_CSD_USER_WP]);
	}

	cmd.opcode = MMC_SET_WRITE_PROT;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;

	for(i = 0; i < loop_count; i++) {
		/* Sending CMD28 foe each WP group size */
		cmd.arg = sector_start_addr + i * wp_group_size;
		err = mmc_wait_for_cmd(card->host, &cmd, 0);
		if(err) {
			goto out;
		}
		/* Sending CMD13 to check card status */
		do {
			err = get_card_status(card, &status, 3);
			if(R1_CURRENT_STATE(status) == R1_STATE_TRAN)
				break;
		} while((!err) && (R1_CURRENT_STATE(status) == R1_STATE_PRG));
		if(err)
			goto out;
	}

	pr_err("%s: sucessed.\n", __func__);
out:

	mmc_release_host(card->host);
	mmc_rpm_release(card->host, &card->dev);
	kfree(ext_csd);
	return err;
}

static struct hd_struct * get_part_from_partname(struct gendisk *g_disk, const char *partname)
{
    struct disk_part_iter piter;
    struct hd_struct *part = NULL;
    
    /* Don't show non-partitionable removable devices or empty devices */
	if(!get_capacity(g_disk) || (!disk_max_parts(g_disk) &&
					(g_disk->flags & GENHD_FL_REMOVABLE) &&
					(g_disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO)))
		return NULL;
        
    /* Show the full disk and all non-0 size partition of it*/
	disk_part_iter_init(&piter, g_disk, DISK_PITER_INCL_PART0);
    
    while((part = disk_part_iter_next(&piter))) {
		if(part->info && part->info->volname[0] &&
				!strncmp(part->info->volname, partname, strlen(partname))) {
				break;
		}

        if (!(part->info) && (strlen(partname) == 0))
            break;
	}
	disk_part_iter_exit(&piter);
    
    return part;
}

static int hw_mmc_part_wp_action(struct block_device *bdev, struct protect_partition_info part_info,
		int (*func)(struct gendisk *disk, struct hd_struct *start_part, struct hd_struct *end_part))
{
	struct gendisk *g_disk = bdev->bd_disk;
	struct hd_struct *start_part = NULL, *end_part =NULL;
	int ret = 0;
	int num_loop = 3;
    char *partname;
    
    partname = part_info.start_partition;
	/* Don't show non-partitionable removable devices or empty devices */
	if(!get_capacity(g_disk) || (!disk_max_parts(g_disk) &&
					(g_disk->flags & GENHD_FL_REMOVABLE) &&
					(g_disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO)))
		return 0;

    start_part = get_part_from_partname(g_disk, part_info.start_partition);
    end_part = get_part_from_partname(g_disk, part_info.end_partition);
    
retry:
    ret = func(g_disk, start_part, end_part);
    if(!ret || (num_loop == 0)) {
		return ret;
	}
	else {
        num_loop--;
        goto retry;
	}

	return ret;
}

static int hw_mmc_set_wp_state(struct block_device *bdev)
{
	int i, ret = 0;

    for(i = 0; i < PARTITION_NUM; i++) {
        if (NULL != protect_partition_list[i].start_partition)
		    ret |= hw_mmc_part_wp_action(bdev, protect_partition_list[i], hw_mmc_do_set_write_protection);
	}

	return ret;
}

#ifdef CONFIG_HUAWEI_KERNEL_DEBUG
static int hw_mmc_get_wp_state(struct block_device *bdev)
{
	int i, ret = 0;

     for(i = 0; i < PARTITION_NUM; i++) {
        if (NULL != protect_partition_list[i].start_partition)
		    ret |= hw_mmc_part_wp_action(bdev, protect_partition_list[i], hw_mmc_do_get_write_protection_info);
	}

	return ret;
}
#endif

int hw_mmc_blk_phy_wp(struct block_device *bdev)
{
	int ret;

	ret = hw_mmc_set_wp_state(bdev);
	if(ret) {
		pr_err("%s: hw mmc set partion wp failed.\n", __func__);
	}
#ifdef CONFIG_HUAWEI_KERNEL_DEBUG
	ret = hw_mmc_get_wp_state(bdev);
	if(ret) {
		pr_err("%s: hw mmc get partion wp info failed.\n", __func__);
	}
#endif
	return ret;
}
EXPORT_SYMBOL(hw_mmc_blk_phy_wp);