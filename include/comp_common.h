#ifndef BCOMP_COMP_COMMON
#define BCOMP_COMP_COMMON

#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/types.h>

/*
WARNING: 
	attached buffers can be allocated only with `kzalloc` or something like that
	(`buffer.data` would be free with `kfree()`)
 */
enum buffer_flags {
	BFA_INITIALIZED, /* buffer initialized */
	BFA_ATTACHED, /* current chunk manages the buffer (buffer initialized) */
};

#define BFA_INIT_FLAG 0

struct buffer {
	unsigned long flags;
	u32 data_sz;
	u32 buf_sz;
	char *data;
};

/*
DOC:
	If `attach_it == 0`: `new_buf_data` doesn't attache to `old_buf`
	Otherwise: `new_buf_data` attach to `old_buf`
 */
void link_data(u32 new_buf_sz, char *new_buf_data, int attach_it,
	       struct buffer *old_buf);

struct chunk {
	struct buffer src;
	struct buffer dst;
};

enum comp_profile { EMPTY, LZ4 };

struct comp_ctx;

struct comp_ops {
	int (*get_private_ctx)(int comp_id, int decomp_id,
			       struct comp_ctx *cctx);
	int (*put_private_ctx)(struct comp_ctx *cctx);
	int (*comp_chunk)(struct comp_ctx *cctx, struct chunk *data);
	int (*decomp_chunk)(struct comp_ctx *cctx, struct chunk *data,
			    u32 expected_sz);
	u32 (*get_dst_buf_sz)(struct comp_ctx *cctx, u32 data_for_comp_sz);
};

struct comp_ctx {
	int comp_prf_id;
	int decomp_prf_id;
	enum comp_profile prf;
	void *private_ctx;
	const struct comp_ops *ops;
};

static inline int comp_src_to_dst(struct chunk *data, struct comp_ctx *ctx)
{
	if (!ctx->ops->comp_chunk)
		return -ENOTSUPP;

	return ctx->ops->comp_chunk(ctx, data);
}

static inline int decomp_src_to_dst(struct chunk *data, u32 expected_sz,
				    struct comp_ctx *ctx)
{
	if (!ctx->ops->decomp_chunk)
		return -ENOTSUPP;

	return ctx->ops->decomp_chunk(ctx, data, expected_sz);
}

/*
DOC:
	comp_dst_buf_size() == 0 means that dst_buf would be allocated inside comp_chunk()
	(dst_buf allocation outside comp_chunk needn't)
*/
static inline u32 comp_dst_buf_size(u32 data_for_comp_sz, struct comp_ctx *ctx)
{
	if (!ctx->ops->get_dst_buf_sz)
		return 0;

	return ctx->ops->get_dst_buf_sz(ctx, data_for_comp_sz);
}

static inline void free_comp(struct comp_ctx *cctx)
{
	if (cctx->private_ctx)
		cctx->ops->put_private_ctx(cctx);

	kfree(cctx);
}

static inline int init_comp(struct comp_ctx *compress, int comp_id,
			    int decomp_id)
{
	if (!compress->ops->get_private_ctx)
		return -ENOTSUPP;

	return compress->ops->get_private_ctx(comp_id, decomp_id, compress);
}

/*	
DOC:
	If (dst_ptr/src_ptr) == NULL -> (dst_ptr/src_ptr) would be allocated, 
	otherwise (dst_buf/src_buf) = (dst_ptr/src_ptr) 
*/
int alloc_chunk(struct chunk **chnk_ptr, u32 dst_sz, u32 src_sz, char *dst_ptr,
		char *src_ptr);
void free_chunk(struct chunk *chnk);

int allocate_chunk_for_comp(struct chunk **chnk_ptr, u32 src_sz, u32 min_dst_sz,
			    struct comp_ctx *cctx);

int init_comp_ops(enum comp_profile cprf, struct comp_ctx *cctx);

#endif /* BCOMP_COMP_COMMON */