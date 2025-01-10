#include <linux/stddef.h>
#include <linux/kstrtox.h>

#include "../include/settings.h"
#include "../include/bcomp.h"

const char NONE[NONE_LEN] = "none";

const enum w_block_size AVAILABLE_BS[BS_N] = { b_4K, b_8K, b_16K, b_32K, b_64K, b_128K };
const char *AVAILABLE_BS_NAMES[BS_STR_LEN] = { "4k", "8k", "16k", "32k", "64k", "128k", NULL };

const enum comp_profile AVAILABLE_CPRF[CPRF_N] = { EMPTY, LZ4 };
const char *AVAILABLE_CPRF_NAMES[CPRF_STR_LEN] = { "empty", "lz4", NULL };

const enum map_profile AVAILABLE_MPRF[MPRF_N] = { LINEAR };
const char *AVAILABLE_MPRF_NAMES[MPRF_STR_LEN] = { "linear", NULL };

const char *get_none_keyword(void)
{
	return NONE;
}

const enum w_block_size *get_available_bs_enum(void)
{
	return AVAILABLE_BS;
}

const char **get_available_bs_names(void)
{
	return AVAILABLE_BS_NAMES;
}

const enum comp_profile *get_available_cprf_enum(void)
{
	return AVAILABLE_CPRF;
}

const char **get_available_cprf_names(void)
{
	return AVAILABLE_CPRF_NAMES;
}

const enum map_profile *get_available_mprf_enum(void)
{
	return AVAILABLE_MPRF;
}

const char **get_available_mprf_names(void)
{
	return AVAILABLE_MPRF_NAMES;
}

void free_user_settings(struct user_settings *settings)
{
	if (settings->path)
		kfree(settings->path);

	kfree(settings);
}

static int _validate_str_to_enum(const char **emum_names, int enum_values_n,
				 int max_str_len, const char *bs_arg, int len,
				 void *enum_res, enum setting_enum_id id)
{
	char *buffer = NULL;
	int ret;

	if (len > max_str_len - 1)
		return -EINVAL;

	buffer = kzalloc(sizeof(*buffer) * max_str_len, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	memcpy(buffer, bs_arg, len);

	for (int i = 0; i < enum_values_n; i++)
		if (!strcmp(emum_names[i], buffer)) {
			switch (id) {
			case BS_ENUM:
				enum w_block_size *bs_res = enum_res;
				(*bs_res) = get_available_bs_enum()[i];
				break;

			case COMP_ENUM:
				enum comp_profile *comp_res = enum_res;
				(*comp_res) = get_available_cprf_enum()[i];
				break;

			case MAP_ENUM:
				enum map_profile *map_res = enum_res;
				(*map_res) = get_available_mprf_enum()[i];
				break;

			default:
				BCOMP_ERRLOG("unexpected setting_enum_id");
				ret = -EINVAL;
				goto end;
			}

			ret = 0;
			goto end;
		}

	ret = -EINVAL;

end:
	kfree(buffer);
	return ret;
}

static int validate_bs(const char *bs_arg, int len, enum w_block_size *bs)
{
	return _validate_str_to_enum(get_available_bs_names(), BS_N, BS_STR_LEN,
				     bs_arg, len, bs, BS_ENUM);
}

static int validate_cprf(const char *cprf_arg, int len, enum comp_profile *cprf)
{
	return _validate_str_to_enum(get_available_cprf_names(), CPRF_N,
				     CPRF_STR_LEN, cprf_arg, len, cprf,
				     COMP_ENUM);
}

static int validate_cprf_id(const char *cprf_id_arg, int len, int *cprf_id)
{
	char buffer[MAX_PRF_ID_STR_LEN] = { 0 };

	if (len > MAX_PRF_ID_STR_LEN - 1)
		return -EINVAL;

	memcpy(buffer, cprf_id_arg, len);
	return kstrtoint(buffer, 10, cprf_id);
}

static int validate_mprf(const char *mprf_arg, int len,
			 enum map_profile *map_prf)
{
	return _validate_str_to_enum(get_available_mprf_names(), MPRF_N,
				     MPRF_STR_LEN, mprf_arg, len, map_prf,
				     MAP_ENUM);
}

static int get_path(const char *path_arg, int len, char **path)
{
	char *p = NULL;
	p = (*path) = kzalloc(sizeof(*p) * (len + 1), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	memcpy(p, path_arg, len);
	return 0;
}

static int str_get_next_delim_idx(const char *arg, int start_idx)
{
	int idx = start_idx;

	while (!IS_STR_END(arg[idx])) {
		if (arg[idx] == DELIMITER)
			return idx;
		idx++;
	}

	return idx;
}

enum parser_stage parse_user_settings(const char *arg,
				      struct user_settings *settings)
{
	// "<4k|...> <empty|...> <int> <linear|...> /dev/<path>"
	int len;
	int cur_idx = 0;
	int sb_idx = cur_idx;
	enum parser_stage stage = BS_STG;

	if (!arg || IS_STR_END(arg[cur_idx])) {
		BCOMP_ERRLOG("settings table shouldn't be empty");
		goto err;
	}

	if (arg[0] == DELIMITER) {
		BCOMP_ERRLOG("the table must not start with a space");
		goto err;
	}

	while (stage != END_STG) {
		cur_idx = str_get_next_delim_idx(arg, sb_idx);
		if ((cur_idx - sb_idx) == 0) {
			BCOMP_ERRLOG("following argument is missing:");
			BCOMP_ERRLOG(STAGE_PP(stage));
			goto err;
		}

		len = cur_idx - sb_idx;

		switch (stage) {
		case BS_STG:
			if (validate_bs(STR_SUFFIX(arg, sb_idx), len,
					&settings->bs))
				goto err;
			stage = COMP_PROFILE_STG;
			break;

		case COMP_PROFILE_STG:
			if (validate_cprf(STR_SUFFIX(arg, sb_idx), len,
					  &settings->cprf))
				goto err;
			stage = COMP_PROFILE_ID_STG;
			break;

		case COMP_PROFILE_ID_STG:
			if (validate_cprf_id(STR_SUFFIX(arg, sb_idx), len,
					     &settings->cprf_id))
				goto err;
			stage = DECOMP_PROFILE_ID_STG;
			break;

		case DECOMP_PROFILE_ID_STG:
			if (validate_cprf_id(STR_SUFFIX(arg, sb_idx), len,
					     &settings->dcprf_id))
				goto err;
			stage = MAP_PROFILE_STG;
			break;

		case MAP_PROFILE_STG:
			if (validate_mprf(STR_SUFFIX(arg, sb_idx), len,
					  &settings->map_prf))
				goto err;
			stage = PATH_STG;
			break;

		case PATH_STG:
			if (get_path(STR_SUFFIX(arg, sb_idx), len,
				     &settings->path))
				goto err;
			stage = END_STG;
			break;

		default:
			BCOMP_ERRLOG("unexpected stage");
			break;
		}
		sb_idx = cur_idx + 1;
	}

	if (!IS_STR_END(arg[cur_idx])) {
		stage = INVALID_STG;
		goto err;
	}

	return stage;

err:
	BCOMP_ERRLOG(
		"bcomp-table should look like:\n<bs> <comp-profile> <comp-prfl-id> <decomp-prfl-id> <map-profile> /dev/<path>");
	BCOMP_ERRLOG(STAGE_PP(stage));
	return stage;
}
