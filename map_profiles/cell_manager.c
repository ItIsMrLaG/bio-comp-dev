#include <linux/fs.h>
#include <linux/gfp_types.h>

#include "../include/bcomp_static.h"
#include "../include/map_common.h"

#include "cell_manager.h"

// TODO:(#REFACTOR) rename file as base_cell_manager

static inline u64
_lba_to_cell_key(sector_t lba, struct base_cell_manager_ctx *base_manager_ctx);

/* ================== CELL ================== */

static int get_base_cell_ptr(struct map_cell **cell_ptr, sector_t lba,
			     void *cell_manager_ctx)
{
	struct base_cell_manager_ctx *base_manager_ctx = cell_manager_ctx;
	u64 cell_key = _lba_to_cell_key(lba, base_manager_ctx);

	BUG_ON(cell_key >= base_manager_ctx->block_number);

	*cell_ptr = base_manager_ctx->storage[cell_key];
	return 0;
}

static int alloc_base_cell(struct map_cell **cell_ptr, sector_t lba,
			   void *cell_manager_ctx)
{
	struct map_cell *cell;
	struct base_cell_manager_ctx *base_manager_ctx = cell_manager_ctx;
	u64 cell_key = _lba_to_cell_key(lba, base_manager_ctx);
	BUG_ON(cell_key >= base_manager_ctx->block_number);

	cell = base_manager_ctx->storage[cell_key];
	if (!cell) {
		cell = kzalloc(sizeof(*cell), GFP_KERNEL);
		if (!cell)
			return -ENOMEM;
	}

	__clear_cell(cell); // TODO:(#REFACTOR) maybe can be removed
	base_manager_ctx->storage[cell_key] = cell;

	*cell_ptr = base_manager_ctx->storage[cell_key];
	return 0;
}

static void _free_base_cell(sector_t cell_key,
			    struct base_cell_manager_ctx *base_manager_ctx)
{
	if (!base_manager_ctx->storage[cell_key])
		return;

	kfree(base_manager_ctx->storage[cell_key]);
}

static void free_base_cell(sector_t lba, void *cell_manager_ctx)
{
	struct base_cell_manager_ctx *base_manager_ctx = cell_manager_ctx;
	u64 cell_key = _lba_to_cell_key(lba, base_manager_ctx);
	BUG_ON(cell_key >= base_manager_ctx->block_number);

	_free_base_cell(cell_key, base_manager_ctx);
}

/* ================== CELL_MANAGER ================== */

static inline u64
_lba_to_cell_key(sector_t lba, struct base_cell_manager_ctx *base_manager_ctx)
{
	u64 bs_in_sectors = DIV_ROUND_UP(base_manager_ctx->bs, 512);
	return DIV_ROUND_UP(lba, bs_in_sectors);
}

static void *alloc_base_cell_manager_ctx(sector_t storage_size,
					 enum w_block_size bs)
{
	struct base_cell_manager_ctx *base_manager_ctx = NULL;
	struct map_cell **storage = NULL;
	u64 bs_in_sectors = DIV_ROUND_UP(bs, 512);
	u64 block_number = DIV_ROUND_UP(storage_size, bs_in_sectors);

	base_manager_ctx = kzalloc(sizeof(*base_manager_ctx), GFP_KERNEL);
	if (!base_manager_ctx) {
		return NULL;
	}

	storage = kzalloc(sizeof(*storage) * block_number, GFP_KERNEL);
	if (!storage)
		goto free_cell_manager;

	base_manager_ctx->block_number = block_number;
	base_manager_ctx->bs = bs;
	base_manager_ctx->storage = storage;

	return base_manager_ctx;

free_cell_manager:
	kfree(base_manager_ctx);
	return NULL;
}

static void free_base_cell_manager_ctx(void *cell_manager_ctx)
{
	struct base_cell_manager_ctx *base_manager_ctx = cell_manager_ctx;
	for (u64 i = 0; i < base_manager_ctx->block_number; i++)
		_free_base_cell(i, cell_manager_ctx);

	kfree(base_manager_ctx);
}

/* ================== GETTER ================== */

const struct cell_manager_ops base_cell_manager_ops = {
	.alloc_cell_manager_ctx = alloc_base_cell_manager_ctx,
	.free_cell_manager_ctx = free_base_cell_manager_ctx,
	.get_cell_ptr = get_base_cell_ptr,
	.alloc_cell = alloc_base_cell,
	.free_cell = free_base_cell,
};

const struct cell_manager_ops *get_base_cell_manager_ops(void)
{
	return &base_cell_manager_ops;
}
