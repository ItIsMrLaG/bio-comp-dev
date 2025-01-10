#include <linux/fs.h>
#include <linux/blkdev.h>

#include "../include/bcomp_static.h"
#include "../include/map_common.h"

#include "cell_manager.h"
#include "liniar_map.h"
#include <linux/printk.h>

const struct cell_manager_ops *LINIAR_MANAGER_OPS;

static int alloc_liniar_private_ctx(struct map_ctx *mctx, sector_t storage_size,
				    enum w_block_size bs)
{
	LINIAR_MANAGER_OPS =
		get_base_cell_manager_ops(); //TODO: mb make as const

	mctx->ops = get_liniar_map_ops();
	mctx->private_ctx =
		alloc_cell_manager_ctx(storage_size, bs, LINIAR_MANAGER_OPS);
	if (!mctx->private_ctx)
		return -ENOMEM;

	return 0;
}

static int free_liniar_private_ctx(struct map_ctx *mctx)
{
	free_cell_manager_ctx(mctx->private_ctx, LINIAR_MANAGER_OPS);
	mctx->private_ctx = NULL;
	return 0;
}

static int update_liniar_cell(struct map_ctx *mctx, sector_t lba, u32 lsize,
			      u32 psize, struct map_cell **cell)
{
	struct map_cell *_cell = NULL;
	int ret;

	if (psize < lsize) {
		ret = alloc_cell(&_cell, lba, mctx->private_ctx,
				 LINIAR_MANAGER_OPS);
		if (ret)
			return ret;
		_cell->lba = lba;
		_cell->pba = lba;
		_cell->lsize = lsize;
		_cell->psize = psize;

	} else {
		ret = get_cell_ptr(&_cell, lba, mctx->private_ctx,
				   LINIAR_MANAGER_OPS);
		if (ret)
			return ret;

		if (_cell != NULL) {
			__clear_cell(_cell);
			_cell->lsize = 0;
			_cell->psize = 1;
		}
	}

	*cell = _cell;
	return 0;
}

static int get_liniar_cell(struct map_ctx *mctx, sector_t lba,
			   struct map_cell **cell)
{
	struct map_cell *_cell = NULL;
	int ret = get_cell_ptr(&_cell, lba, mctx->private_ctx,
			       LINIAR_MANAGER_OPS);
	if (ret)
		return ret;

	*cell = _cell;
	if (_cell == NULL || (_cell->lsize < _cell->psize))
		*cell = NULL;

	return 0;
}

/* ================== GETTER ================== */

const struct map_ops liniar_map_ops = {
	.alloc_private_ctx = alloc_liniar_private_ctx,
	.free_private_ctx = free_liniar_private_ctx,
	.update_cell = update_liniar_cell,
	.get_cell = get_liniar_cell
};

const struct map_ops *get_liniar_map_ops(void)
{
	return &liniar_map_ops;
}
