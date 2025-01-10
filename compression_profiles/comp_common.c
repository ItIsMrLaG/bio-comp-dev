#include <linux/fs.h>
#include <linux/gfp_types.h>

#include "../include/bcomp_static.h"
#include "../include/comp_common.h"

#include "empty_comp.h"
#include "lz4_comp.h"

static inline int __init_buf(unsigned long flgs, struct buffer *buf,
			     unsigned int buf_sz, char *ptr)
{
	char *data;

	if (test_bit(BFA_INITIALIZED, &flgs) && test_bit(BFA_ATTACHED, &flgs)) {
		data = kzalloc(sizeof(*buf) * buf_sz, GFP_NOIO);
		if (!data)
			return -ENOMEM;
		goto init;
	}

	if (test_bit(BFA_INITIALIZED, &flgs) &&
	    !test_bit(BFA_ATTACHED, &flgs)) {
		data = ptr;
		goto init;
	}

	if (!test_bit(BFA_INITIALIZED, &flgs)) {
		data = NULL;
		goto init;
	}

	return -EINVAL;

init:
	buf->flags = flgs;
	buf->data_sz = 0;
	buf->buf_sz = buf_sz;
	buf->data = data;

	return 0;
}

void link_data(u32 new_buf_sz, char *new_buf_data, int attach_it,
	       struct buffer *old_buf)
{
	if (test_bit(BFA_ATTACHED, &old_buf->flags) && old_buf->data != NULL)
		kfree(old_buf->data);

	old_buf->flags = BFA_INIT_FLAG;
	assign_bit(BFA_INITIALIZED, &old_buf->flags, true);
	if (attach_it)
		assign_bit(BFA_ATTACHED, &old_buf->flags, true);

	old_buf->data = new_buf_data;
	old_buf->buf_sz = new_buf_sz;
	old_buf->data_sz = 0;
}

void free_chunk(struct chunk *chnk)
{
	/* FREE DST_BUF */
	if (test_bit(BFA_INITIALIZED, &(chnk->dst.flags)) &&
	    test_bit(BFA_ATTACHED, &(chnk->dst.flags)) &&
	    chnk->dst.data != NULL)
		kfree(chnk->dst.data);

	/* FREE SRC_BUF */
	if (test_bit(BFA_INITIALIZED, &(chnk->src.flags)) &&
	    test_bit(BFA_ATTACHED, &(chnk->src.flags)) &&
	    chnk->src.data != NULL)
		kfree(chnk->src.data);

	kfree(chnk);
}

static inline void __reset_chunk(struct chunk *chnk)
{
	memset(chnk, 0, sizeof(*chnk));
	chnk->src.data = BFA_INIT_FLAG;
	chnk->dst.data = BFA_INIT_FLAG;
}

static inline int __get_chunk_flags(unsigned long *flgs_ptr, unsigned int sz,
				    char *ptr)
{
	unsigned long flgs = 0;

	if (sz != 0 && ptr != NULL) {
		assign_bit(BFA_INITIALIZED, &flgs, true);
		assign_bit(BFA_ATTACHED, &flgs, false);

		(*flgs_ptr) = flgs;
		return 0;
	}

	if (sz == 0 && ptr == NULL) {
		assign_bit(BFA_INITIALIZED, &flgs, false);
		assign_bit(BFA_ATTACHED, &flgs, false);

		(*flgs_ptr) = flgs;
		return 0;
	}

	if (sz != 0 && ptr == NULL) {
		assign_bit(BFA_INITIALIZED, &flgs, true);
		assign_bit(BFA_ATTACHED, &flgs, true);

		(*flgs_ptr) = flgs;
		return 0;
	}

	return -EINVAL;
}

int alloc_chunk(struct chunk **chnk_ptr, u32 dst_sz, u32 src_sz, char *dst_ptr,
		char *src_ptr)
{
	unsigned long dst_flgs;
	unsigned long src_flgs;
	struct chunk *chnk;
	int ret;

	chnk = kzalloc(sizeof(struct chunk), GFP_NOIO);
	if (!chnk)
		return -ENOMEM;

	ret = __get_chunk_flags(&dst_flgs, dst_sz, dst_ptr);
	if (ret)
		goto err;

	ret = __init_buf(dst_flgs, &(chnk->dst), dst_sz, dst_ptr);
	if (ret)
		goto err;

	ret = __get_chunk_flags(&src_flgs, src_sz, src_ptr);
	if (ret)
		goto err_free_dst;

	ret = __init_buf(src_flgs, &(chnk->src), src_sz, src_ptr);
	if (ret)
		goto err_free_dst;

	*chnk_ptr = chnk;
	return 0;

err_free_dst:
	if (test_bit(BFA_INITIALIZED, &(chnk->dst.flags)) &&
	    test_bit(BFA_ATTACHED, &(chnk->dst.flags))) {
		kfree(chnk->dst.data);
	}
err:
	return ret;
}

int allocate_chunk_for_comp(struct chunk **chnk_ptr, u32 src_sz, u32 min_dst_sz,
			    struct comp_ctx *cctx)
{
	u32 dst_size;
	int ret;

	dst_size = comp_dst_buf_size(src_sz, cctx);
	if (dst_size)
		dst_size = max_t(u32, dst_size, min_dst_sz);

	ret = alloc_chunk(chnk_ptr, dst_size, src_sz, NULL, NULL);
	if (ret)
		return ret;

	return 0;
}

int init_comp_ops(enum comp_profile cprf, struct comp_ctx *cctx)
{
	switch (cprf) {
	case EMPTY:
		cctx->ops = get_empty_comp_ops();
		break;
	case LZ4:
		cctx->ops = get_lz4_comp_ops();
		break;
	default:
		return -EINVAL;
	}

	cctx->prf = cprf;
	return 0;
}
