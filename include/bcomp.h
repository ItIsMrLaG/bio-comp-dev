#ifndef BCOMP_MODULE
#define BCOMP_MODULE
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/blk_types.h>

/* ========= REQUEST STRUCTURES ========= */

#include "bcomp_static.h"
#include "settings.h"
#include "map_common.h"
#include "comp_common.h"
#include "stats.h"

struct bcomp_req {
	enum req_op op_type;
	struct bio *original_bio;

	struct map_entity *entity;
	struct bcomp_dev *bcdev;
};

struct underlying_dev {
	struct block_device *bdev;
	struct file *bdev_fl;
	struct bio_set *bset;
};

struct bcomp_dev {
	enum w_block_size bs;
	struct gendisk *bcomp_disk;
	struct underlying_dev *under_dev;
	struct comp_ctx *compress;
	struct map_ctx *map;
	struct stats *stats;
};

// ======== initialization ======== //

int bcomp_alloc_dev(struct bcomp_dev **dev_pointer);
int bcomp_init_dev(struct user_settings *settings, int major, int free_minor,
		   struct bcomp_dev *bcdev);
void bcomp_free_dev(struct bcomp_dev *bcdev);

// ======== data-path ======== //

/* -------- tools -------- */
void copy_sg_to_buf(struct buffer *buf, struct bio *bio);
void copy_buf_to_sg(struct buffer *buf, struct bio *bio);
int add_buffer_to_bio(struct buffer *buf, u32 part_to_use, struct bio *bio);

/* -------- request -------- */
struct bcomp_req *bcomp_alloc_req(void);
void bcomp_free_req(struct bcomp_req *req);

/* -------- bio -------- */
void bcomp_submit_bio(struct bio *original_bio);

#endif /* BCOMP_MODULE */