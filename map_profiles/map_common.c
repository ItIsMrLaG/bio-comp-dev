#include <linux/fs.h>
#include <linux/gfp_types.h>
#include <linux/blk_types.h>
#include <linux/bio.h>
#include <linux/blkdev.h>

#include "../include/bcomp_static.h"
#include "../include/map_common.h"
#include "../include/comp_common.h"
#include "../include/bcomp.h"

#include "liniar_map.h"

/* ================== MAP_CTX ================== */

int init_map_ops(enum map_profile map_prf, struct map_ctx *mctx)
{
	switch (map_prf) {
	case LINEAR:
		mctx->ops = get_liniar_map_ops();
		break;
	default:
		return -EINVAL;
	}

	mctx->prf = map_prf;
	return 0;
}
