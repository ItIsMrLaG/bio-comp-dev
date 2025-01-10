#ifndef BCOMP_MAP_COMMON
#define BCOMP_MAP_COMMON

#include <linux/bitops.h>
#include <linux/types.h>

#include "bcomp_static.h"
#include "comp_common.h"

enum map_entity_flags { ENTITY_DATA_INITED, ENTITY_CELL_INITED };

//All map_cell belongs to map_ctx->private_ctx
struct map_cell {
	u32 lsize; // user expected size
	u32 psize; // actual stored size

	sector_t lba;
	sector_t pba;
};

struct map_entity {
	unsigned long flags;

	sector_t lba; // if cell after mapping == NULL: lba == pba

	struct map_cell *cell; //doesn't belong to map_entity
	struct chunk *data; // doesn't belong to map_entity
};

#define is_data_compressed(cell) ((cell) != NULL)

#define get_map_entity_pba(entity_ptr)                           \
	(((entity_ptr)->cell == NULL) ? (entity_ptr)->cell_key : \
					(entity_ptr)->cell.pba)

static inline void add_cell_to_entity(struct map_cell *cell,
				      struct map_entity *entity)
{
	assign_bit(ENTITY_CELL_INITED, &entity->flags, true);
	entity->cell = cell;
}

static inline void add_data_to_entity(struct chunk *data,
				      struct map_entity *entity)
{
	assign_bit(ENTITY_DATA_INITED, &entity->flags, true);
	entity->data = data;
}

struct map_ctx;

enum map_profile { LINEAR };

struct map_ops {
	int (*alloc_private_ctx)(struct map_ctx *mctx, sector_t storage_size,
				 enum w_block_size bs);
	int (*free_private_ctx)(struct map_ctx *mctx);
	int (*update_cell)(struct map_ctx *mctx, sector_t lba, u32 lsize,
			   u32 psize, struct map_cell **cell);
	int (*get_cell)(struct map_ctx *mctx, sector_t lba,
			struct map_cell **cell);
	//TODO: extend interface to work with non-linear mapping and rewrite all pipline
};

struct map_ctx {
	enum map_profile prf;
	void *private_ctx;
	const struct map_ops *ops;
};

static inline void free_map(struct map_ctx *map)
{
	if (map->private_ctx)
		map->ops->free_private_ctx(map);

	kfree(map);
}

static inline int init_map(struct map_ctx *map, sector_t storage_size,
			   enum w_block_size bs)
{
	if (!map->ops->alloc_private_ctx)
		return -EEXIST;

	return map->ops->alloc_private_ctx(map, storage_size, bs);
}

static inline int update_mapping(struct map_cell **cell, sector_t lba,
				 u32 lsize, u32 psize, struct map_ctx *mctx)
{
	if (!mctx->ops->update_cell)
		return -EEXIST;

	return mctx->ops->update_cell(mctx, lba, lsize, psize, cell);
}

static inline int get_mapping(struct map_cell **cell, sector_t lba,
			      struct map_ctx *mctx)
{
	if (!mctx->ops->get_cell)
		return -EEXIST;

	return mctx->ops->get_cell(mctx, lba, cell);
}

int init_map_ops(enum map_profile map_prf, struct map_ctx *mctx);

#endif /* BCOMP_MAP_COMMON */