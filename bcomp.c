// SPDX-License-Identifier: GPL-2.0-only
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/bio.h>
#include <linux/blkdev.h>

#define BCOMP_NAME "bio-comp-dev"
#define bcomp_log(msg) pr_info("%s: %s\n", BCOMP_NAME, msg)

struct bcomp_bdev {
	struct gendisk *bcomp_disk;
	struct gendisk *map_disk;
};

static int bcomp_major;

static struct bcomp_bdev *bcomp_dev = NULL;

static void bcomp_submit_bio(struct bio *bio);

static void bcomp_bdevs_free();

static void bcomp_bdev_free(struct bcomp_bdev *dev);

static int bcomp_bdev_alloc(const char *map_disk_path, struct bcomp_bdev *dev);

static const struct block_device_operations brd_fops = {
	.owner = THIS_MODULE,
	.submit_bio = bcomp_submit_bio,
};

// ======== params ======== //

static int bcomp_disk_create(const char *arg, const struct kernel_param *kp)
{
	int ret;

	if (bcomp_dev != NULL) {
		bcomp_log("device already exists");
		return -EACCES;
	}

	ret = bcomp_bdev_alloc(arg, bcomp_dev);

	return ret;
}

static int bcomp_disk_info(char *buf, const struct kernel_param *kp)
{
	ssize_t len;
	char disk_name[33] = { 0 };

	if (bcomp_dev == NULL) {
		bcomp_log("no device for unmapping");
		return -ENODEV;
	}

	for (int i = 0; i < 32; i++){
		disk_name[i] = bcomp_dev->map_disk->disk_name[i];
	}

	strcpy(buf, disk_name);

	return 0;
}

static const struct kernel_param_ops bcomp_map_ops = {
	.set = bcomp_disk_create,
	.get = bcomp_disk_info,
};

MODULE_PARM_DESC(bcomp_mapper, "Create bcomp dev (map)");
module_param_cb(bcomp_mapper, &bcomp_map_ops, NULL, S_IRUGO | S_IWUSR);

static int bcomp_disk_delete(const char *arg, const struct kernel_param *kp)
{
	if (bcomp_dev == NULL) {
		bcomp_log("no device for unmapping");
		return -ENODEV;
	}

	bcomp_bdev_free(bcomp_dev);

	bcomp_log("device unmapped");

	return 0;
}

static const struct kernel_param_ops bcomp_unmap_ops = {
	.set = bcomp_disk_delete,
	.get = NULL,
};

MODULE_PARM_DESC(bcomp_unmapper, "Delete bcomp dev (unmap)");
module_param_cb(bcomp_unmapper, &bcomp_unmap_ops, NULL, S_IWUSR);

// ======== module ======== //

static int __init bcomp_init(void)
{
	int ret;

	bcomp_major = register_blkdev(0, BCOMP_NAME);

	if (bcomp_major < 0) {
		bcomp_log("module NOT loaded");
		return -EIO;
	}

	bcomp_log("module loaded");
	return 0;
}

static void __exit bcomp_exit(void)
{
	unregister_blkdev(bcomp_major, BCOMP_NAME);
	bcomp_bdevs_free();

	bcomp_log("module unloaded");
}

MODULE_AUTHOR("Georgy Sichkar <mail4egor@gmail.com>");
MODULE_LICENSE("GPL");
