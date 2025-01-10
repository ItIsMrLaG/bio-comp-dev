#include <linux/stddef.h>
#include <linux/fs.h>

#include "../include/comp_common.h"

#include "empty_comp.h"

static int empty_get_private_ctx(int comp_id, int decomp_id,
				 struct comp_ctx *cctx)
{
	cctx->comp_prf_id = comp_id;
	cctx->decomp_prf_id = decomp_id;
	cctx->ops = get_empty_comp_ops();
	cctx->private_ctx = NULL;

	return 0;
}

static int empty_put_private_ctx(struct comp_ctx *)
{
	return 0;
}

static void _remap_src_to_dst(struct chunk *chnk)
{
	assign_bit(BFA_INITIALIZED, &(chnk->dst.flags), true);
	assign_bit(BFA_ATTACHED, &(chnk->dst.flags), false);
	chnk->dst.buf_sz = chnk->src.buf_sz;
	chnk->dst.data_sz = chnk->src.data_sz;
	chnk->dst.data = chnk->src.data;
}

static int empty_cmpress_chunk(struct comp_ctx *cctx, struct chunk *chnk)
{
	BUG_ON(!test_bit(BFA_INITIALIZED, &(chnk->src.flags)));
	_remap_src_to_dst(chnk);
	return 0;
}

static int empty_decmpress_chunk(struct comp_ctx *cctx, struct chunk *chnk,
				 u32 expexted_sz)
{
	_remap_src_to_dst(chnk);
	return 0;
}

const struct comp_ops empty_comp_ops = {
	.get_private_ctx = empty_get_private_ctx,
	.put_private_ctx = empty_put_private_ctx,
	.comp_chunk = empty_cmpress_chunk,
	.decomp_chunk = empty_decmpress_chunk,
	.get_dst_buf_sz = NULL
};

const struct comp_ops *get_empty_comp_ops(void)
{
	return &empty_comp_ops;
}
