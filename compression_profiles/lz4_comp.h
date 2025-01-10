#ifndef LZ4_COMP
#define LZ4_COMP

#include <linux/lz4.h>

#include "../include/comp_common.h"

#define BCOMP_LZ4_MAX_FAST_ID 15 // [0..15] <=> acceleration factor
#define BCOMP_LZ4_MAX_HC_ID      \
	(BCOMP_LZ4_MAX_FAST_ID + \
	 LZ4HC_MAX_CLEVEL) // [16..31] <=> [1..16] HC-compressionLevel

enum comp_tp { BCOMP_LZ4_TP_FAST, BCOMP_LZ4_TP_HC };

#define BCOMP_LZ4_GET_COMP_TP(comp_prf)                            \
	((comp_prf) <= BCOMP_LZ4_MAX_FAST_ID ? BCOMP_LZ4_TP_FAST : \
	 (comp_prf) <= BCOMP_LZ4_MAX_HC_ID   ? BCOMP_LZ4_TP_HC :   \
					       10000)

#define get_lz4_hc_id(id) (id - BCOMP_LZ4_MAX_FAST_ID)

enum decomp_tp { BCOMP_LZ4_DECOM_FAST = 0, BCOMP_LZ4_DECOM_SAFE = 1 };

const struct comp_ops *get_lz4_comp_ops(void);

#endif /* LZ4_COMP */