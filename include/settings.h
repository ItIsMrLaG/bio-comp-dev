#ifndef BCOMP_MODULE_SETTINGS
#define BCOMP_MODULE_SETTINGS

#include "map_common.h"
#include "comp_common.h"
#include "bcomp_static.h"

#define DELIMITER ' '
#define IS_STR_END(symb) ((symb) == '\0')
#define STR_SUFFIX(start_ptr, shift) ((start_ptr) + (shift))

#define NONE_LEN 5
const char *get_none_keyword(void);

#define BS_N 6
#define BS_STR_LEN 10
const enum w_block_size *get_available_bs_enum(void);
const char **get_available_bs_names(void);

#define CPRF_N 2
#define CPRF_STR_LEN 10
const enum comp_profile *get_available_cprf_enum(void);
const char **get_available_cprf_names(void);

#define MPRF_N 1
#define MPRF_STR_LEN 10
const enum map_profile *get_available_mprf_enum(void);
const char **get_available_mprf_names(void);

#define MAX_PRF_ID_STR_LEN 10

enum setting_enum_id { BS_ENUM, COMP_ENUM, MAP_ENUM };

struct user_settings {
	enum w_block_size bs;
	enum comp_profile cprf;
	int cprf_id;
	int dcprf_id;
	enum map_profile map_prf;
	char *path;
};

enum parser_stage {
	BS_STG,
	COMP_PROFILE_STG,
	COMP_PROFILE_ID_STG,
	DECOMP_PROFILE_ID_STG,
	MAP_PROFILE_STG,
	PATH_STG,
	END_STG,
	INVALID_STG
};
#define STAGE_PP(stage)                                             \
	((stage) == BS_STG		? "block-size (bs)" :       \
	 (stage) == COMP_PROFILE_STG	? "compress profile" :      \
	 (stage) == COMP_PROFILE_ID_STG ? "compress profile id" :   \
	 (stage) == DECOMP_PROFILE_ID_STG ? "decompress profile id" :   \
	 (stage) == MAP_PROFILE_STG	? "map profile" :           \
	 (stage) == END_STG		? "unprented stage (END)" : \
					  "unexpected stage")

void free_user_settings(struct user_settings *settings);

enum parser_stage parse_user_settings(const char *arg,
				      struct user_settings *settings);

#endif /* BCOMP_MODULE_SETTINGS */
