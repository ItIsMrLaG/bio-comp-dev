#include <linux/types.h>
#include <linux/blk_types.h>

#include "../include/stats.h"

void reset_stats(struct stats *stats)
{
	memset(stats, 0, sizeof(struct stats));
}

enum compression_level get_compression_level(u32 compressed_sz, u32 source_sz)
{
	double coef = compressed_sz * 100;

	if (coef < LESS_25_P * source_sz)
		return LESS_25_P;

	if (coef < LESS_50_P * source_sz)
		return LESS_50_P;

	if (coef < LESS_75_P * source_sz)
		return LESS_75_P;

	return LESS_99_P;
}
