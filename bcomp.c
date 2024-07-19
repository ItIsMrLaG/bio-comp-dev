// SPDX-License-Identifier: GPL-2.0-only
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/bio.h>
#include <linux/blkdev.h>

#define BCOMP_NAME "bio-comp-dev"
#define INIT_MINOR 0
#define POOL_SIZE 1024
#define bcomp_log(msg) pr_info("%s: %s\n", BCOMP_NAME, msg)

struct bcomp_bdev {
	struct gendisk *bcomp_disk;
	struct block_device *underlying_bdev;
	struct bio_set *bs;
};

struct bcomp_meta_info {
	struct bio *original_bio;
};

static int bcomp_major;

static int bcomp_free_minor = INIT_MINOR;

static struct bcomp_bdev *bcomp_dev = NULL;

static void bcomp_bdev_free(struct bcomp_bdev *dev);

static void bcomp_free(void);

static int bcomp_bdev_alloc(const char *map_disk_path, struct bcomp_bdev **dev);

static void bcomp_submit_bio(struct bio *original_bio);

static void bcomp_bio_end_io(struct bio *clone_bio);

static const struct block_device_operations bcomp_fops = {
	.owner = THIS_MODULE,
	.submit_bio = bcomp_submit_bio,
};

static void bcomp_bdev_free(struct bcomp_bdev *dev)
{
	del_gendisk(dev->bcomp_disk);
	put_disk(dev->bcomp_disk);

	blkdev_put(
		dev->underlying_bdev,
		FMODE_WRITE |
			FMODE_READ); //TODO: FMODE_WRITE | FMODE_READ or something else

	bioset_exit(dev->bs);
	kfree(dev->bs);

	kfree(dev);
}

static void bcomp_free(void)
{
	if (bcomp_dev == NULL)
		return;

	bcomp_bdev_free(bcomp_dev);
	bcomp_dev = NULL;
}

//TODO: rewrite -> bcomp_bdev_init (without allocation)
static int bcomp_bdev_alloc(const char *map_disk_path, struct bcomp_bdev **d)
{
	struct gendisk *disk;
	struct bcomp_bdev *dev;
	struct block_device *bdev;
	struct bio_set *bs;
	char buf[DISK_NAME_LEN];
	int ret;

	//TODO: Is in my case holder == dev? (is it correct)
	bdev = blkdev_get_by_path(map_disk_path, FMODE_WRITE | FMODE_READ,
				  (*d));
	if (IS_ERR(bdev)) {
		ret = PTR_ERR(bdev);
		goto exit_fail;
	}

	if (!bdev->bd_disk->fops->submit_bio) {
		ret = -EPERM;
		goto exit_fail;
	}

	dev = (*d) = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto exit_fail;
	}

	dev->underlying_bdev = bdev;

	bs = kzalloc(sizeof(*bs), GFP_KERNEL);
	if (!bs) {
		ret = -ENOMEM;
		goto problem_with_bio_set;
	}

	bioset_init(bs, POOL_SIZE, 0, BIOSET_NEED_BVECS);

	disk = dev->bcomp_disk = blk_alloc_disk(NUMA_NO_NODE);
	if (!disk) {
		ret = -ENOMEM;
		goto problem_with_disk;
	}

	disk->major = bcomp_major;
	disk->first_minor = bcomp_free_minor;
	++bcomp_free_minor;
	disk->fops = &bcomp_fops;
	disk->private_data = dev;

	dev->bcomp_disk = disk;
	snprintf(buf, DISK_NAME_LEN, "bcomp%d", disk->first_minor);
	strscpy(disk->disk_name, buf, DISK_NAME_LEN);
	ret = add_disk(disk);
	if (ret)
		goto out_cleanup_disk;

	return 0;

out_cleanup_disk:
	put_disk(disk);
problem_with_disk:
	bioset_exit(bs);
	kfree(bs);
problem_with_bio_set:
	kfree(dev);
	(*d) = NULL;
exit_fail:
	return ret;
}

static void bcomp_submit_bio(struct bio *original_bio)
{
	struct bcomp_bdev *dev = original_bio->bi_bdev->bd_disk->private_data;
	struct bio *bio_clone =
		bio_alloc_clone(dev->underlying_bdev, original_bio, GFP_NOIO,
				dev->bs); //TODO: GFP_NOIO or something else?

	struct bcomp_meta_info *meta = kzalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta) {
		//TODO: -ENOMEM handling???
		original_bio->bi_end_io(original_bio);
		return;
	}

	meta->original_bio = original_bio;

	bio_clone->bi_end_io = bcomp_bio_end_io;
	bio_clone->bi_private = meta;

	dev->underlying_bdev->bd_disk->fops->submit_bio(bio_clone);
}

static void bcomp_bio_end_io(struct bio *clone_bio)
{
	struct bcomp_meta_info *meta = clone_bio->bi_private;
	struct bio *original_bio = meta->original_bio;

	kfree(meta);
	original_bio->bi_end_io(original_bio);
	bio_put(clone_bio);
}

// ======== params ======== //

static int bcomp_disk_create(const char *arg, const struct kernel_param *kp)
{
	int ret;

	if (bcomp_dev != NULL) {
		bcomp_log("device already exists");
		return -EACCES;
	}

	ret = bcomp_bdev_alloc(arg, &bcomp_dev);

	return ret;
}

static int bcomp_disk_info(char *buf, const struct kernel_param *kp)
{
	if (bcomp_dev == NULL) {
		bcomp_log("no device for unmapping");
		return -ENODEV;
	}

	strscpy(buf, bcomp_dev->underlying_bdev->bd_disk->disk_name,
		DISK_NAME_LEN);

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
	bcomp_free();

	bcomp_log("module unloaded");
}

MODULE_AUTHOR("Georgy Sichkar <mail4egor@gmail.com>");
MODULE_LICENSE("GPL");
