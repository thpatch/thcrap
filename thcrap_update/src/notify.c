/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * Update notifications for thcrap and the game itself.
  */

#include <thcrap.h>

static const char *update_url_message =
	"The new version can be found at\n"
	"\n"
	"\t";
static const char *mbox_copy_message =
	"\n"
	"\n"
	"(Press Ctrl+C to copy the text of this message box and the URL)";

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
	json_t *run_cfg = runconfig_get();
	DWORD min_build = json_object_get_hex(run_cfg, "thcrap_version_min");
	int ret = min_build > PROJECT_VERSION();
	if(ret) {
		char format[11];
		const char *url_engine = json_object_get_string(run_cfg, "thcrap_url");
		str_hexdate_format(format, min_build);
		log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
			"A new version (%s) of the %s is available.\n"
			"\n"
			"This update contains new features and important bug fixes "
			"for your current patch configuration.%s%s%s%s",
			format, PROJECT_NAME(),
			url_engine ? "\n\n": "",
			url_engine ? update_url_message : "",
			url_engine ? url_engine : "",
			url_engine ? mbox_copy_message : ""
		);
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
		const char *url_update = json_object_get_string(run_cfg, "url_update");
		log_mboxf("Old version detected", MB_OK | MB_ICONINFORMATION,
			"You are running an old version of %s (%s).\n"
			"\n"
			"While %s is confirmed to work with this version, we recommend updating "
			"your game to the latest official version (%s).%s%s%s%s",
			json_string_value(title), json_string_value(build), PROJECT_NAME_SHORT(),
			json_string_value(latest),
			url_update ? "\n\n": "",
			url_update ? update_url_message : "",
			url_update ? url_update : "",
			url_update ? mbox_copy_message : ""
		);
	}
	return ret;
}
