/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Update notifications for thcrap and the game itself.
  */

#include <thcrap.h>
#include "self.h"

/// Self-updating messages
/// ----------------------
static UINT self_msg_type[] = {
	MB_ICONINFORMATION, // SELF_OK
	MB_ICONINFORMATION, // SELF_NO_PUBLIC_KEY
	MB_ICONEXCLAMATION, // SELF_SERVER_ERROR
	MB_ICONEXCLAMATION, // SELF_DISK_ERROR
	MB_ICONERROR, // SELF_NO_SIG
	MB_ICONERROR, // SELF_SIG_FAIL
	MB_ICONINFORMATION, // SELF_REPLACE_ERROR
};

static const char *self_header_failure =
	"A new version (${build}) of the ${project} is available.\n"
	"\n";

static const char *self_body[] = {
	// SELF_OK
	"The ${project} has been successfully updated to version ${build}.\n"
	"\n"
	"For further information about this new release, visit\n"
	"\n"
	"\t${desc_url}",
	// SELF_NO_PUBLIC_KEY
	"Due to the lack of a digital signature on the currently running "
	"patcher build, an automatic update was not attempted.\n"
	"\n"
	"You can manually download the update archive from\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"However, we can't prove its authenticity. Be careful!",
	// SELF_SERVER_ERROR
	"An automatic update was attempted, but none of the download servers "
	"could be reached.\n"
	"\n"
	"The new version can be found at\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"However, instead of manually downloading the archive, we recommend "
	"to repeat this automatic update later, as this process also checks "
	"the digital signature on the archive for authenticity.",
	// SELF_DISK_ERROR
	"An automatic update was attempted, but the update archive could not "
	"be saved to disk, possibly due to a lack of writing permissions in "
	"the ${project_short} directory (${thcrap_dir}).\n"
	"\n"
	"You can manually download the update archive from\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"Its digital signature has already been verified to be authentic.",
	// SELF_NO_SIG
	"An automatic update was attempted, but the server did not provide a "
	"digital signature to prove the authenticity of the update archive.\n"
	"\n"
	"Thus, it may have been maliciously altered.",
	// SELF_SIG_FAIL
	"An automatic update was attempted, but the digital signature of the "
	"update archive could not be verified against the public key on the "
	"currently running patcher build.\n"
	"\n"
	"This means that the update has been maliciously altered.",
	// SELF_REPLACE_ERROR
	"An automatic update was attempted, but the current build could not "
	"be replaced with the new one, possibly due to a lack of writing "
	"permissions.\n"
	"\n"
	"Please manually extract the new version from the update archive that "
	"has been saved to your ${project_short} directory "
	"(${thcrap_dir}${arc_fn}). "
	"Its digital signature has already been verified to be authentic.\n"
	"\n"
	"For further information about this new release, visit\n"
	"\n"
	"\t${desc_url}"
};

const char *self_sig_error =
	"\n"
	"We advise against downloading it from the originating website until "
	"this problem has been resolved.";
/// ----------------------

/// Game build messages
/// -------------------
const char *oldbuild_title = "Old version detected";

const char *oldbuild_header =
	"You are running an old version of ${game_title} (${build_running}).\n"
	"\n";

const char *oldbuild_maybe_supported =
	"${project_short} may or may not work with this version, so we recommend updating to the latest official version (${build_latest}).";

const char *oldbuild_url =
	"\n"
	"\n"
	"You can download the update at\n"
	"\n"
	"\t${url_update}\n"
	"\n"
	"(Press Ctrl+C to copy the text of this message box and the URL)";
/// -------------------

int IsLatestBuild(json_t *build, json_t **latest)
{
	json_t *json_latest = json_object_get(runconfig_get(), "latest");
	size_t i;
	if(!json_is_string(build) || !latest || !json_latest) {
		return -1;
	}
	json_flex_array_foreach(json_latest, i, *latest) {
		if(json_equal(build, *latest)) {
			return 1;
		}
	}
	return 0;
}

int update_notify_thcrap(void)
{
	const size_t SELF_MSG_SLOT = (size_t)self_body;
	self_result_t ret = SELF_OK;
	json_t *run_cfg = runconfig_get();

	json_t *patches = json_object_get(run_cfg, "patches");
	size_t i;
	json_t *patch_info;
	uint32_t min_build = 0;
	json_array_foreach(patches, i, patch_info) {
		auto cur_min_build = json_object_get_hex(patch_info, "thcrap_version_min");
		min_build = max(min_build, cur_min_build);
	}
	if(min_build > PROJECT_VERSION()) {
		const char *thcrap_dir = json_object_get_string(run_cfg, "thcrap_dir");
		const char *thcrap_url = json_object_get_string(run_cfg, "thcrap_url");
		char *arc_fn = NULL;
		const char *self_msg = NULL;
		char min_build_str[11];
		str_hexdate_format(min_build_str, min_build);
		ret = self_update(thcrap_dir, &arc_fn);
		strings_sprintf(SELF_MSG_SLOT,
			"%s%s",
			ret != SELF_OK ? self_header_failure : "",
			self_body[ret]
		);
		if(ret == SELF_NO_SIG || ret == SELF_SIG_FAIL) {
			strings_strcat(SELF_MSG_SLOT, self_sig_error);
		}
		strings_replace(SELF_MSG_SLOT, "${project}", PROJECT_NAME());
		strings_replace(SELF_MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT());
		strings_replace(SELF_MSG_SLOT, "${build}", min_build_str);
		strings_replace(SELF_MSG_SLOT, "${thcrap_dir}", thcrap_dir);
		strings_replace(SELF_MSG_SLOT, "${desc_url}", thcrap_url);
		self_msg = strings_replace(SELF_MSG_SLOT, "${arc_fn}", arc_fn);
		log_mboxf(NULL, MB_OK | self_msg_type[ret], self_msg);
		SAFE_FREE(arc_fn);
	}
	return ret;
}

int update_notify_game(void)
{
	json_t *run_cfg = runconfig_get();
	const json_t *title = runconfig_title_get();
	json_t *build = json_object_get(run_cfg, "build");
	json_t *latest = NULL;
	int ret;
	
	if(!json_is_string(build) || !json_is_string(title)) {
		return -1;
	}
	ret = IsLatestBuild(build, &latest) == 0 && json_is_string(latest);
	if(ret) {
		const size_t MSG_SLOT = (size_t)oldbuild_header;
		unsigned int msg_type = MB_ICONINFORMATION;
		auto *url_update = json_object_get_string(run_cfg, "url_update");

		strings_strclr(MSG_SLOT);
		strings_strcat(MSG_SLOT, oldbuild_header);
		strings_strcat(MSG_SLOT, oldbuild_maybe_supported);
		if(url_update) {
			strings_strcat(MSG_SLOT, oldbuild_url);
		}

		strings_replace(MSG_SLOT, "${game_title}", json_string_value(title));
		strings_replace(MSG_SLOT, "${build_running}", json_string_value(build));
		strings_replace(MSG_SLOT, "${build_latest}", json_string_value(latest));
		strings_replace(MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT());
		auto *msg = strings_replace(MSG_SLOT, "${url_update}", url_update);

		log_mbox(oldbuild_title, MB_OK | msg_type, msg);
	}
	return ret;
}
