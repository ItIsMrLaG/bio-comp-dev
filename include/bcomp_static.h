#ifndef BCOMP_STATIC
#define BCOMP_STATIC

#define BCOMP_NAME "bio-comp-dev"
#define INIT_MINOR 0
#define SINGLETON_DISK_LEN 10
#define POOL_SIZE 1024

#define BCOMP_LOG(fmt, ...) \
	pr_info("%s[inf] " fmt "\n", BCOMP_NAME, ##__VA_ARGS__)

#define BCOMP_ERRLOG(fmt, ...) \
	pr_err("%s[err] " fmt "\n", BCOMP_NAME, ##__VA_ARGS__)

#define bcomp_dbg(fmt, ...) \
	pr_debug("%s[dbg] " fmt "\n", BCOMP_NAME, ##__VA_ARGS__)

#define SUPPORTED_BS w_BS(4)
#define w_BS(k) ((k) * 1024)

enum w_block_size {
	b_4K = w_BS(4),
	b_8K = w_BS(8),
	b_16K = w_BS(16),
	b_32K = w_BS(32),
	b_64K = w_BS(64),
	b_128K = w_BS(128)
};

#endif /* BCOMP_STATIC */
