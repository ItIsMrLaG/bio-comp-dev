#ifndef BCOMP_MAP_BASE_CELL_MANAGER
#define BCOMP_MAP_BASE_CELL_MANAGER

#include <linux/types.h>

#include "../include/bcomp_static.h"
#include "../include/map_common.h"

static inline void __clear_cell(struct map_cell *cell)
{
	memset(cell, 0, sizeof(*cell));
}

struct cell_manager_ops {
	void *(*alloc_cell_manager_ctx)(sector_t storage_size,
					enum w_block_size bs);
	void (*free_cell_manager_ctx)(void *cell_manager_ctx);
	int (*get_cell_ptr)(struct map_cell **cell_ptr, sector_t lba,
			    void *cell_manager_ctx);
	int (*alloc_cell)(struct map_cell **cell_ptr, sector_t lba,
			  void *cell_manager_ctx);
	void (*free_cell)(u64 cell_key, void *cell_manager_ctx);
};

static inline void *alloc_cell_manager_ctx(sector_t storage_size,
					   enum w_block_size bs,
					   const struct cell_manager_ops *ops)
{
	if (!ops || !ops->alloc_cell_manager_ctx)
		return NULL;

	return ops->alloc_cell_manager_ctx(storage_size, bs);
}

static inline void free_cell_manager_ctx(void *cell_manager_ctx,
					 const struct cell_manager_ops *ops)
{
	if (!ops || !ops->free_cell_manager_ctx)
		return;

	ops->free_cell_manager_ctx(cell_manager_ctx);
}

static inline int get_cell_ptr(struct map_cell **cell_ptr, sector_t lba,
			       void *cell_manager_ctx,
			       const struct cell_manager_ops *ops)
{
	if (!ops || !ops->get_cell_ptr) {
		*cell_ptr = NULL;
		return 0;
	}

	return ops->get_cell_ptr(cell_ptr, lba, cell_manager_ctx);
}

static inline int alloc_cell(struct map_cell **cell_ptr, sector_t lba,
			     void *cell_manager_ctx,
			     const struct cell_manager_ops *ops)
{
	if (!ops || !ops->alloc_cell) {
		*cell_ptr = NULL;
		return 0;
	}

	return ops->alloc_cell(cell_ptr, lba, cell_manager_ctx);
}

static inline void free_cell(u64 cell_key, void *cell_manager_ctx,
			     const struct cell_manager_ops *ops)
{
	if (!ops || !ops->free_cell)
		return;

	ops->free_cell(cell_key, cell_manager_ctx);
}

struct base_cell_manager_ctx {
	/* 
	IMPORTANT:
		Array of struct map_cell pointers.
		storage[cell_key] == NULL <=> physical block-i -- uncompressed block
		storage[cell_key] != NULL <=> physical block-i -- compressed block (corresponding map_cell contains mapping)
	
		- cel_key == lba

	TODO:(#NONLINEAR) [ rewrite as resizable storage ]
		(it should be another data structure as tree or something else)
	 */
	u64 block_number;
	enum w_block_size bs;
	struct map_cell **storage;
};

const struct cell_manager_ops *get_base_cell_manager_ops(void);

#endif /* BCOMP_MAP_BASE_CELL_MANAGER */