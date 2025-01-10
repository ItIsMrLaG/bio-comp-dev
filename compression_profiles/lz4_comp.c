#include <uapi/linux/stddef.h>
#include <linux/fs.h>
#include <linux/lz4.h>
#include <linux/vmalloc.h>

#include "../include/bcomp_static.h"
#include "../include/comp_common.h"

#include "lz4_comp.h"

static int validate_comp_prf_id(int comp_id)
{
	if (comp_id <= BCOMP_LZ4_MAX_FAST_ID)
		return 0;
	if (comp_id <= BCOMP_LZ4_MAX_HC_ID)
		return 0;
	return -EINVAL;
}

static int validate_decomp_prf_id(int decomp_id)
{
	if (decomp_id == BCOMP_LZ4_DECOM_FAST ||
	    decomp_id == BCOMP_LZ4_DECOM_SAFE)
		return 0;
	return -EINVAL;
}

static void *alloc_wrkmem(int profile_id)
{
	if (profile_id <= BCOMP_LZ4_MAX_FAST_ID)
		return vzalloc(LZ4_MEM_COMPRESS);

	if (profile_id <= BCOMP_LZ4_MAX_HC_ID)
		return vzalloc(LZ4HC_MEM_COMPRESS);

	return NULL;
}

static int lz4_get_private_ctx(int comp_id, int decomp_id,
			       struct comp_ctx *cctx)
{
	char *wrkmem;
	int ret;

	ret = validate_comp_prf_id(comp_id);
	if (ret)
		return ret;

	ret = validate_decomp_prf_id(decomp_id);
	if (ret)
		return ret;

	wrkmem = alloc_wrkmem(comp_id);
	if (!wrkmem)
		return -ENOMEM;

	cctx->comp_prf_id = comp_id;
	cctx->decomp_prf_id = decomp_id;
	cctx->prf = LZ4;
	cctx->ops = get_lz4_comp_ops();
	cctx->private_ctx = wrkmem;

	return 0;
}

static int lz4_put_private_ctx(struct comp_ctx *cctx)
{
	vfree(cctx->private_ctx);
	return 0;
}

static int validate_chunk(struct chunk *chnk)
{
	if (!test_bit(BFA_INITIALIZED, &(chnk->src.flags))) {
		BCOMP_ERRLOG("src.data not initialized");
		return -EIO;
	}

	if (!test_bit(BFA_INITIALIZED, &(chnk->dst.flags))) {
		BCOMP_ERRLOG("dst.data not initialized");
		return -EIO;
	}

	if (chnk->src.data_sz > LZ4_MAX_INPUT_SIZE) {
		BCOMP_ERRLOG("src.data_sz > LZ4_MAX_INPUT_SIZE");
		return -EIO;
	}

	return 0;
}

static int fast_compress(struct chunk *chnk, int id, void *wrkmem)
{
	return chnk->dst.data_sz = LZ4_compress_fast(
		       chnk->src.data, chnk->dst.data, chnk->src.data_sz,
		       chnk->dst.buf_sz, id, wrkmem);
}

static int hc_compress(struct chunk *chnk, int id, void *wrkmem)
{
	return chnk->dst.data_sz = LZ4_compress_HC(
		       chnk->src.data, chnk->dst.data, chnk->src.data_sz,
		       chnk->dst.buf_sz, id, wrkmem);
}

static int compress(int comp_id, struct chunk *chnk, void *wrkmem)
{
	int ret;

	enum comp_tp tp = BCOMP_LZ4_GET_COMP_TP(comp_id);
	switch (tp) {
	case BCOMP_LZ4_TP_FAST:
		ret = fast_compress(chnk, comp_id, wrkmem);
		break;

	case BCOMP_LZ4_TP_HC:
		ret = hc_compress(chnk, get_lz4_hc_id(comp_id), wrkmem);
		break;

	default:
		return -ENOTSUPP;
	}

	if (!ret)
		return -EIO;

	chnk->dst.data_sz = ret;

	return 0;
}

static int lz4_cmpress_chunk(struct comp_ctx *cctx, struct chunk *chnk)
{
	int ret;

	ret = validate_chunk(chnk);
	if (ret)
		return ret;

	ret = compress(cctx->comp_prf_id, chnk, cctx->private_ctx);
	if (ret) {
		BCOMP_ERRLOG("problem with LZ4_compress");
		return ret;
	}

	return 0;
}

static int fast_decompress(struct chunk *chnk, u32 expexted_sz)
{
	chnk->dst.data_sz = expexted_sz;
	int ret = LZ4_decompress_fast(chnk->src.data, chnk->dst.data,
				      expexted_sz);

	return ret;
}

static int safe_decompress(struct chunk *chnk)
{
	return chnk->dst.data_sz =
		       LZ4_decompress_safe(chnk->src.data, chnk->dst.data,
					   chnk->src.data_sz, chnk->dst.buf_sz);
}

static int decompress(int decomp_prf_id, struct chunk *chnk, u32 expexted_sz)
{
	int ret;

	enum decomp_tp tp = decomp_prf_id;

	switch (tp) {
	case BCOMP_LZ4_DECOM_FAST:
		ret = fast_decompress(chnk, expexted_sz);
		break;

	case BCOMP_LZ4_DECOM_SAFE:
		ret = safe_decompress(chnk);
		break;

	default:
		return -ENOTSUPP;
	}

	if (ret <= 0)
		return -EIO;

	if (chnk->dst.data_sz != expexted_sz)
		return -EIO;

	return 0;
}

static int lz4_decmpress_chunk(struct comp_ctx *cctx, struct chunk *chnk,
			       u32 expexted_sz)
{
	int ret;

	ret = validate_chunk(chnk);
	if (ret)
		return ret;

	ret = decompress(cctx->decomp_prf_id, chnk, expexted_sz);
	if (ret) {
		BCOMP_ERRLOG("problem with LZ4_decompress");
		return ret;
	}

	return 0;
}

static u32 lz4_get_dst_buf_sz(struct comp_ctx *cctx, u32 data_for_comp_sz)
{
	return LZ4_compressBound(data_for_comp_sz);
}

const struct comp_ops lz4_comp_ops = { .get_private_ctx = lz4_get_private_ctx,
				       .put_private_ctx = lz4_put_private_ctx,
				       .comp_chunk = lz4_cmpress_chunk,
				       .decomp_chunk = lz4_decmpress_chunk,
				       .get_dst_buf_sz = lz4_get_dst_buf_sz };

const struct comp_ops *get_lz4_comp_ops(void)
{
	return &lz4_comp_ops;
}
