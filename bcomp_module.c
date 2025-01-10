// SPDX-License-Identifier: GPL-2.0-only
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/types.h>

#include "include/bcomp.h"
#include "include/settings.h"
#include "include/stats.h"

static int bcomp_major;

static int bcomp_free_minor = INIT_MINOR;

static struct bcomp_dev *bcomp_dev = NULL;

// ======== creation ======== //

static int bcomp_disk_create(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;
	struct user_settings *settings = NULL;
	struct bcomp_dev *bcdev = NULL;

	if (bcomp_dev != NULL) {
		BCOMP_ERRLOG("device already exists");
		return -EACCES;
	}

	settings = kzalloc(sizeof(*settings), GFP_KERNEL);
	if (!settings) {
		BCOMP_ERRLOG("can't alloc settings");
		return -ENOMEM;
	}

	if (parse_user_settings(arg, settings) != END_STG) {
		ret = -EINVAL;
		goto free_settings;
	}
	BCOMP_LOG("device settings parsed");

	ret = bcomp_alloc_dev(&bcdev);
	if (ret)
		goto free_settings;

	ret = bcomp_init_dev(settings, bcomp_major, bcomp_free_minor, bcdev);
	if (ret)
		goto free_dev;
	BCOMP_LOG("device initialized");

	++bcomp_free_minor;
	bcomp_dev = bcdev;

	free_user_settings(settings);

	BCOMP_LOG("device mapped");
	BCOMP_LOG(arg);
	return 0;

free_dev:
	bcomp_free_dev(bcdev);
free_settings:
	free_user_settings(settings);
	return ret;
}

static int bcomp_disk_info(char *buf, const struct kernel_param *kp)
{
	if (bcomp_dev == NULL) {
		BCOMP_ERRLOG("no mapped device");
		return -ENODEV;
	}

	return sysfs_emit(buf, "%s:%s\n", bcomp_dev->bcomp_disk->disk_name,
			  bcomp_dev->under_dev->bdev->bd_disk->disk_name);
}

static const struct kernel_param_ops bcomp_map_ops = {
	.set = bcomp_disk_create,
	.get = bcomp_disk_info,
};

static int bcomp_disk_delete(const char *arg, const struct kernel_param *kp)
{
	if (bcomp_dev == NULL) {
		BCOMP_ERRLOG("no device for unmapping");
		return -ENODEV;
	}

	bcomp_free_dev(bcomp_dev);
	bcomp_dev = NULL;
	--bcomp_free_minor;

	BCOMP_LOG("device unmapped");

	return 0;
}

static const struct kernel_param_ops bcomp_unmap_ops = {
	.set = bcomp_disk_delete,
	.get = NULL,
};

static int bcomp_reset_stats(const char *arg, const struct kernel_param *kp)
{
	reset_stats(bcomp_dev->stats);
	return 0;
}

static int bcomp_get_stats(char *buf, const struct kernel_param *kp)
{
	struct stats *st;

	if (bcomp_dev == NULL || bcomp_dev->stats == NULL) {
		return -ENODEV;
	}

	st = bcomp_dev->stats;
	return sysfs_emit(buf, PRITTY_STATS_TEMPLATE,
			  atomic64_read(&st->compressed_reqs_cnt_25),
			  atomic64_read(&st->compressed_reqs_cnt_50),
			  atomic64_read(&st->compressed_reqs_cnt_75),
			  atomic64_read(&st->compressed_reqs_cnt_99),
			  atomic64_read(&st->uncompressed_reqs_cnt),
			  atomic64_read(&st->all_reqs_cnt),
			  atomic64_read(&st->data_in_bytes),
			  atomic64_read(&st->compressed_data_in_bytes));
}

static const struct kernel_param_ops bcomp_stats_ops = {
	.set = bcomp_reset_stats,
	.get = bcomp_get_stats,
};

// ======== module ======== //

static int __init bcomp_init(void)
{
	bcomp_major = register_blkdev(0, BCOMP_NAME);

	if (bcomp_major < 0) {
		BCOMP_ERRLOG("module NOT loaded");
		return -EIO;
	}

	BCOMP_LOG("module loaded");
	return 0;
}

static void __exit bcomp_exit(void)
{
	unregister_blkdev(bcomp_major, BCOMP_NAME);

	if (bcomp_dev == NULL) {
		return;
	}

	bcomp_free_dev(bcomp_dev);
	bcomp_dev = NULL;

	BCOMP_LOG("module unloaded");
}

MODULE_PARM_DESC(bcomp_stats, "Bcomp dev statistics");
module_param_cb(bcomp_stats, &bcomp_stats_ops, NULL, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(bcomp_mapper, "Create bcomp dev (map)");
module_param_cb(bcomp_mapper, &bcomp_map_ops, NULL, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(bcomp_unmapper, "Delete bcomp dev (unmap)");
module_param_cb(bcomp_unmapper, &bcomp_unmap_ops, NULL, S_IWUSR);

MODULE_AUTHOR("Georgy Sichkar <mail4egor@gmail.com>");
MODULE_LICENSE("GPL");

module_init(bcomp_init);
module_exit(bcomp_exit);
