#ifndef BCOMP_STATS
#define BCOMP_STATS

#include <linux/types.h>

enum compression_level {
	LESS_25_P = 25,
	LESS_50_P = 50,
	LESS_75_P = 75,
	LESS_99_P = 99
};

struct stats {
	atomic64_t all_reqs_cnt;
	atomic64_t uncompressed_reqs_cnt;

	atomic64_t data_in_bytes;
	atomic64_t compressed_data_in_bytes;

	atomic64_t compressed_reqs_cnt_25; // 0% <= compressed_data < 25%
	atomic64_t compressed_reqs_cnt_50; // 25% <= compressed_data < 50%
	atomic64_t compressed_reqs_cnt_75; // 50% <= compressed_data < 75%
	atomic64_t compressed_reqs_cnt_99; // 75% <= compressed_data < 100%
};

#define PRITTY_STATS_TEMPLATE \
	"\
compressed_reqs_cnt_25: %lld\n\
compressed_reqs_cnt_50: %lld\n\
compressed_reqs_cnt_75: %lld\n\
compressed_reqs_cnt_99: %lld\n\
uncompressed_reqs_cnt: %lld\n\
all_reqs_cnt: %lld\n\
data_in_bytes: %lld\n\
compressed_data_in_bytes: %lld\n\
"

void reset_stats(struct stats *stats);

enum compression_level get_compression_level(u32 compressed_sz, u32 source_sz);

#endif // BCOMP_STATS