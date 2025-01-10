#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/types.h>

#include "include/bcomp.h"
#include "include/map_common.h"
#include "include/comp_common.h"
#include "include/settings.h"
#include "include/stats.h"

// ======== initialization ======== //

static const struct block_device_operations bcomp_fops = {
	.owner = THIS_MODULE,
	.submit_bio = bcomp_submit_bio,
};

static void free_under_dev(struct underlying_dev *under_dev)
{
	if (under_dev->bdev_fl)
		bdev_fput(under_dev->bdev_fl);

	if (under_dev->bset) {
		bioset_exit(under_dev->bset);
		kfree(under_dev->bset);
	}

	kfree(under_dev);
}

static int init_under_dev(const char *map_disk_path,
			  struct underlying_dev *under_dev)
{
	struct file *fbdev;
	struct block_device *bdev;
	struct bio_set *bset;

	fbdev = bdev_file_open_by_path(
		map_disk_path, BLK_OPEN_READ | BLK_OPEN_WRITE, under_dev, NULL);
	if (IS_ERR(fbdev)) {
		BCOMP_ERRLOG("incorrect path");
		return PTR_ERR(fbdev);
	}

	bdev = file_bdev(fbdev);

	under_dev->bdev = bdev;
	under_dev->bdev_fl = fbdev;

	bset = kzalloc(sizeof(*bset), GFP_KERNEL);
	if (!bset)
		return -ENOMEM;

	bioset_init(bset, POOL_SIZE, 0, BIOSET_NEED_BVECS);
	under_dev->bset = bset;

	return 0;
}

static void free_disk(struct gendisk *disk)
{
	del_gendisk(disk);
	put_disk(disk);
}

static void init_disk(struct gendisk *disk, struct bcomp_dev *bcdev, int major,
		      int free_minor)
{
	disk->major = major;
	disk->first_minor = free_minor;
	disk->minors = 1;
	disk->fops = &bcomp_fops;
	disk->private_data = bcdev;

	disk->flags |= GENHD_FL_NO_PART;

	set_capacity(disk, get_capacity(bcdev->under_dev->bdev->bd_disk));

	snprintf(disk->disk_name, DISK_NAME_LEN, "bcomp%d", disk->first_minor);
}

int bcomp_alloc_dev(struct bcomp_dev **dev_pointer)
{
	struct bcomp_dev *bcdev;
	struct gendisk *disk;
	struct underlying_dev *under_dev;
	struct comp_ctx *cctx;
	struct map_ctx *mctx;
	struct stats *stats;

	bcdev = (*dev_pointer) = kzalloc(sizeof(*bcdev), GFP_KERNEL);
	if (!bcdev)
		goto dev_alloc_err;

	under_dev = kzalloc(sizeof(*under_dev), GFP_KERNEL);
	if (!under_dev)
		goto under_dev_alloc_err;

	disk = blk_alloc_disk(NULL, NUMA_NO_NODE);
	if (!disk)
		goto disk_alloc_err;

	cctx = kzalloc(sizeof(*cctx), GFP_KERNEL);
	if (!cctx)
		goto comp_ctx_alloc_err;

	mctx = kzalloc(sizeof(*mctx), GFP_KERNEL);
	if (!mctx)
		goto map_ctx_alloc_err;

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		goto stats_alloc_err;

	bcdev->bcomp_disk = disk;
	bcdev->under_dev = under_dev;
	bcdev->compress = cctx;
	bcdev->map = mctx;
	bcdev->stats = stats;

	return 0;

stats_alloc_err:
	kfree(mctx);
map_ctx_alloc_err:
	kfree(cctx);
comp_ctx_alloc_err:
	put_disk(bcdev->bcomp_disk);
disk_alloc_err:
	kfree(under_dev);
under_dev_alloc_err:
	kfree(bcdev);
dev_alloc_err:
	return -ENOMEM;
}

void bcomp_free_dev(struct bcomp_dev *bcdev)
{
	if (bcdev->bcomp_disk) {
		free_disk(bcdev->bcomp_disk);
		bcdev->bcomp_disk = NULL;
	}

	if (bcdev->under_dev) {
		free_under_dev(bcdev->under_dev);
		bcdev->under_dev = NULL;
	}

	if (bcdev->compress) {
		free_comp(bcdev->compress);
		bcdev->compress = NULL;
	}

	if (bcdev->map) {
		free_map(bcdev->map);
		bcdev->map = NULL;
	}

	if (bcdev->stats) {
		kfree(bcdev->stats);
	}

	kfree(bcdev);
}

int bcomp_init_dev(struct user_settings *settings, int major, int free_minor,
		   struct bcomp_dev *bcdev)
{
	int ret;

	ret = init_map_ops(settings->map_prf, bcdev->map);
	if (ret) {
		BCOMP_ERRLOG("current map profile not implemented");
		return ret;
	}

	ret = init_comp_ops(settings->cprf, bcdev->compress);
	if (ret) {
		BCOMP_ERRLOG("current compress profile not implemented");
	}

	ret = init_under_dev(settings->path, bcdev->under_dev);
	if (ret) {
		BCOMP_ERRLOG("underlying dev init");
		return ret;
	}

	ret = init_comp(bcdev->compress, settings->cprf_id, settings->dcprf_id);
	if (ret) {
		BCOMP_ERRLOG("compression profile init");
		return ret;
	}

	ret = init_map(bcdev->map,
		       get_capacity(bcdev->under_dev->bdev->bd_disk),
		       settings->bs);
	if (ret) {
		BCOMP_ERRLOG("map profile init");
		return ret;
	}

	bcdev->bs = settings->bs;

	init_disk(bcdev->bcomp_disk, bcdev, major, free_minor);

	return add_disk(bcdev->bcomp_disk);
}

// ======== data-path ======== //

/* -------- tools -------- */

void copy_sg_to_buf(struct buffer *buf, struct bio *bio)
{
	struct bio_vec bv;
	struct bvec_iter iter;
	char *ptr = buf->data;

	bio_for_each_segment(bv, bio, iter) {
		memcpy_from_bvec(ptr, &bv);
		ptr += bv.bv_len;
	}

	buf->data_sz = bio->bi_iter.bi_size;
}

void copy_buf_to_sg(struct buffer *buf, struct bio *bio)
{
	struct bio_vec bv;
	struct bvec_iter iter;
	char *ptr = buf->data;

	bio_for_each_segment(bv, bio, iter) {
		memcpy_to_bvec(&bv, ptr);
		ptr += bv.bv_len;
	}

	buf->data_sz = bio->bi_iter.bi_size;
}

int add_buffer_to_bio(struct buffer *buf, u32 part_to_use, struct bio *bio)
{
	unsigned int pageoff, pagelen;
	unsigned int len = part_to_use;
	char *data = buf->data;
	int ret;

	if (!test_bit(BFA_INITIALIZED, &(buf->flags))) {
		BCOMP_ERRLOG("(chunk->dst.data) wait initialization");
		return -EINVAL;
	}

	if (buf->buf_sz < part_to_use) {
		BCOMP_ERRLOG("(chunk->dst.data) wait initialization");
		return -EINVAL;
	}

	pageoff = offset_in_page(data);
	pagelen = min_t(unsigned int, len, PAGE_SIZE - pageoff);

	ret = bio_add_page(bio, virt_to_page(data), pagelen, pageoff);
	if (ret != pagelen) {
		BCOMP_ERRLOG("bio_add_page err");
		return -EAGAIN;
	}

	len -= pagelen;
	data += pagelen;

	while (len) {
		pagelen = min_t(unsigned int, len, PAGE_SIZE);

		ret = bio_add_page(bio, virt_to_page(data), pagelen, 0);
		if (ret != pagelen) {
			BCOMP_ERRLOG("bio_add_page err");
			return -EAGAIN;
		}

		len -= pagelen;
		data += pagelen;
	}

	return 0;
}

static inline int __init_req_op(enum req_op *req_op_type, enum req_op op_type)
{
	switch (op_type) {
	case REQ_OP_READ:
		(*req_op_type) = REQ_OP_READ;
		return 0;

	case REQ_OP_WRITE:
		(*req_op_type) = REQ_OP_WRITE;
		return 0;

	default:
		return -ENOTSUPP;
	}
}

static inline unsigned int __bio_size_to_bio_pages(struct bio *bio)
{
	sector_t pages =
		DIV_ROUND_UP_SECTOR_T(bio_sectors(bio), PAGE_SIZE / 512);

	return min_t(sector_t, pages, BIO_MAX_VECS);
}

/* -------- request -------- */

struct bcomp_req *bcomp_alloc_req(void)
{
	struct bcomp_req *req = kzalloc(sizeof(*req), GFP_NOIO);
	if (!req)
		return NULL;

	req->entity = kzalloc(sizeof(*req->entity), GFP_NOIO);
	if (!req->entity) {
		kfree(req);
		return NULL;
	}

	return req;
}

void bcomp_free_req(struct bcomp_req *req)
{
	if (req->entity)
		kfree(req->entity);

	kfree(req);
}

static void _free_req_with_chunk(struct bcomp_req *req)
{
	if (test_bit(ENTITY_DATA_INITED, &req->entity->flags) &&
	    req->entity->data != NULL)
		free_chunk(req->entity->data);

	bcomp_free_req(req);
}

static struct bcomp_req *_create_req(enum req_op op_type,
				     struct bcomp_dev *bcdev,
				     struct bio *original_bio,
				     int (*init_entity)(struct bcomp_req *req))
{
	struct bcomp_req *req = bcomp_alloc_req();
	if (!req)
		return NULL;

	req->op_type = op_type;
	req->bcdev = bcdev;
	req->original_bio = original_bio;

	if (init_entity(req)) {
		bcomp_free_req(req);
		return NULL;
	}

	return req;
}

/* -------- init-write-request -------- */

static void write_req_update_statistics(struct stats *stats,
					struct bcomp_req *req)
{
	atomic64_t *compressed_reqs_cnt;

	atomic64_inc(&stats->all_reqs_cnt);
	atomic64_add(req->entity->data->src.data_sz, &stats->data_in_bytes);

	if ((test_bit(ENTITY_CELL_INITED, &req->entity->flags) &&
	     req->entity->cell == NULL) ||
	    req->bcdev->compress->prf == EMPTY) {
		atomic64_inc(&stats->uncompressed_reqs_cnt);
	} else {
		if (!test_bit(ENTITY_CELL_INITED, &req->entity->flags)) {
			pr_err("Impossible ENTITY_CELL_FLAG");
			goto end;
		}

		atomic64_add(req->entity->data->dst.data_sz,
			     &stats->compressed_data_in_bytes);

		switch (get_compression_level(req->entity->cell->psize,
					      req->entity->cell->lsize)) {
		case LESS_25_P:
			compressed_reqs_cnt = &stats->compressed_reqs_cnt_25;
			break;

		case LESS_50_P:
			compressed_reqs_cnt = &stats->compressed_reqs_cnt_50;
			break;

		case LESS_75_P:
			compressed_reqs_cnt = &stats->compressed_reqs_cnt_75;
			break;

		case LESS_99_P:
			compressed_reqs_cnt = &stats->compressed_reqs_cnt_99;
			break;

		default:
			pr_err("Impossible compressed_reqs_cnt in statistic");
			goto end;
		}

end:
		atomic64_inc(compressed_reqs_cnt);
	}
}

static void write_req_endio(struct bio *bio)
{
	struct bcomp_req *req = bio->bi_private;

	req->original_bio->bi_status = bio->bi_status;

	if (bio->bi_status == BLK_STS_OK)
		write_req_update_statistics(req->bcdev->stats, req);

	bio_endio(req->original_bio);

	_free_req_with_chunk(req);
	bio_put(bio);
}

static int write_req_init_entity(struct bcomp_req *req)
{
	struct chunk *chnk;
	struct map_cell *cell;
	struct bcomp_dev *bcdev = req->bcdev;
	struct bio *original_bio = req->original_bio;
	unsigned int payload_size = original_bio->bi_iter.bi_size;
	unsigned int lba = original_bio->bi_iter.bi_sector;
	int ret;

	/*
	IMPORTANT:
		Writing to the underlying device must occur in blocks 
		equal to req->bcdev->bs.

		TODO:(#NONLINEAR) [ minds about RMW ]
			how to implement the layout stage into the current architecture
			(combine many cell-chunk into one buffer, then write it)
	*/

	/* ALLOCATION */
	ret = allocate_chunk_for_comp(&chnk, payload_size, bcdev->bs,
				      bcdev->compress);
	if (ret)
		return ret;

	copy_sg_to_buf(&chnk->src, original_bio);

	/* COMMPRESSION */
	ret = comp_src_to_dst(chnk, bcdev->compress);
	if (ret) {
		BCOMP_ERRLOG("Compression failed");
		goto free_chnk;
	}

	/* MAPPING */
	BUG_ON(!test_bit(BFA_INITIALIZED, &(chnk->dst.flags)));
	ret = update_mapping(&cell, lba, chnk->src.data_sz, chnk->dst.data_sz,
			     bcdev->map);
	if (ret) {
		BCOMP_ERRLOG("compression: Map failed");
		goto free_chnk;
	}

	if (!is_data_compressed(cell)) {
		link_data(chnk->src.buf_sz, chnk->src.data, false, &chnk->dst);
		chnk->dst.data_sz = chnk->src.data_sz;
	}

	/* MAP_ENTITY INITIALIZATION */
	req->entity->lba = lba;
	add_data_to_entity(chnk, req->entity);
	add_cell_to_entity(cell, req->entity);

	return 0;

free_chnk:
	free_chunk(chnk);
	return ret;
}

static blk_status_t write_req_submit(enum req_op op_type,
				     struct bio *original_bio)
{
	struct bcomp_dev *bcdev = original_bio->bi_bdev->bd_disk->private_data;
	struct bcomp_req *req;
	struct bio *new_bio;
	sector_t pba;
	blk_status_t status;

	new_bio = bio_alloc(bcdev->under_dev->bdev,
			    __bio_size_to_bio_pages(original_bio), op_type,
			    GFP_NOIO);
	if (!new_bio)
		return BLK_STS_RESOURCE;

	req = _create_req(op_type, bcdev, original_bio, write_req_init_entity);
	if (!req) {
		status = BLK_STS_IOERR;
		goto put_new_bio;
	}

	if (add_buffer_to_bio(&req->entity->data->dst, bcdev->bs, new_bio)) {
		status = BLK_STS_IOERR;
		goto free_write_req;
	}

	if (is_data_compressed(req->entity->cell))
		pba = req->entity->cell->pba;
	else
		pba = original_bio->bi_iter.bi_sector;

	new_bio->bi_end_io = write_req_endio;
	new_bio->bi_private = req;
	new_bio->bi_iter.bi_sector = pba;

	submit_bio_noacct(new_bio);
	return BLK_STS_OK;

free_write_req:
	_free_req_with_chunk(req);
put_new_bio:
	bio_put(new_bio);
	return status;
}

/* -------- read-request -------- */

static void read_req_endio(struct bio *bio)
{
	struct bcomp_req *req = bio->bi_private;
	struct chunk *chnk = req->entity->data;
	struct map_cell *cell = req->entity->cell;
	struct bio *original_bio = req->original_bio;

	original_bio->bi_status = bio->bi_status;

	if (bio->bi_status != BLK_STS_OK)
		goto end_original_bio;

	if (is_data_compressed(cell)) {
		chnk->src.data_sz = cell->psize;
		if (decomp_src_to_dst(chnk, cell->lsize,
				      req->bcdev->compress)) {
			original_bio->bi_status = BLK_STS_IOERR;
			goto end_original_bio;
		}

		copy_buf_to_sg(&(chnk->dst), original_bio);
	}

end_original_bio:
	bio_endio(original_bio);

	_free_req_with_chunk(req);
	bio_put(bio);
}

static int read_req_init_entity(struct bcomp_req *req)
{
	struct chunk *chnk;
	struct map_cell *cell;
	struct bcomp_dev *bcdev = req->bcdev;
	struct bio *original_bio = req->original_bio;
	unsigned int lba = original_bio->bi_iter.bi_sector;
	int ret;

	/* MAPPING */
	ret = get_mapping(&cell, lba, bcdev->map);
	if (ret) {
		BCOMP_ERRLOG("decompression: Map failed");
		return ret;
	}

	if (is_data_compressed(cell)) {
		/*
		IMPORTANT:
			Reading from the underlying device occurs in fixed-size blocks, 
			so the size of the src.data must be equal to req->bcdev->bs.
		*/
		ret = alloc_chunk(&chnk, cell->lsize, req->bcdev->bs, NULL,
				  NULL);
		if (ret)
			return ret;

		add_data_to_entity(chnk, req->entity);
	}

	req->entity->lba = lba;
	add_cell_to_entity(cell, req->entity);
	return 0;
}

static blk_status_t read_req_submit(enum req_op op_type,
				    struct bio *original_bio)
{
	struct bcomp_dev *bcdev = original_bio->bi_bdev->bd_disk->private_data;
	struct bio *new_bio;
	struct bcomp_req *req;
	sector_t pba;
	blk_status_t status;

	req = _create_req(op_type, bcdev, original_bio, read_req_init_entity);
	if (!req)
		return BLK_STS_IOERR;

	BUG_ON(!test_bit(ENTITY_CELL_INITED, &req->entity->flags));

	if (is_data_compressed(req->entity->cell)) {
		new_bio = bio_alloc(bcdev->under_dev->bdev,
				    __bio_size_to_bio_pages(original_bio),
				    op_type, GFP_NOIO);

		if (!new_bio) {
			status = BLK_STS_RESOURCE;
			goto free_read_req;
		}

		if (add_buffer_to_bio(&req->entity->data->src, bcdev->bs,
				      new_bio)) {
			status = BLK_STS_IOERR;
			goto free_bio;
		}

		pba = req->entity->cell->pba;

	} else {
		new_bio = bio_alloc_clone(bcdev->under_dev->bdev, original_bio,
					  GFP_NOIO, bcdev->under_dev->bset);

		if (!new_bio) {
			status = BLK_STS_RESOURCE;
			goto free_read_req;
		}

		pba = original_bio->bi_iter.bi_sector;
		new_bio->bi_iter.bi_size = original_bio->bi_iter.bi_size;
	}

	new_bio->bi_end_io = read_req_endio;
	new_bio->bi_private = req;
	new_bio->bi_iter.bi_sector = pba;

	submit_bio_noacct(new_bio);

	return BLK_STS_OK;

free_bio:
	bio_put(new_bio);
free_read_req:
	_free_req_with_chunk(req);
	return status;
}

/* -------- bio -------- */

void bcomp_submit_bio(struct bio *original_bio)
{
	struct bcomp_dev *bcdev = original_bio->bi_bdev->bd_disk->private_data;
	enum req_op op_type = bio_op(original_bio);

	if (original_bio->bi_iter.bi_size != bcdev->bs) {
		/*
		TODO:(#MINDIT) [ implemetation features, SUPPORTED_BS ]
			current implementation of READ and WRITE correctly works only while 
			* original_bio->bi_iter.bi_size == SUPPORTED_BS
			* SUPPORTED_BS == req->bcdev->bs
		*/
		BCOMP_ERRLOG("unsupported block size");
		goto submit_bio_with_err;
	}

	switch (op_type) {
	case REQ_OP_WRITE:
		if (write_req_submit(op_type, original_bio) == BLK_STS_OK)
			return;
		goto submit_bio_with_err;

	case REQ_OP_READ:
		if (read_req_submit(op_type, original_bio) == BLK_STS_OK)
			return;
		goto submit_bio_with_err;

	default:
		goto submit_bio_with_err;
	}

	// ERROR CASE:
submit_bio_with_err:
	bio_io_error(original_bio);
}
