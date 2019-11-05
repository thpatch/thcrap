/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Update notifications for thcrap itself.
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
	MB_ICONERROR, // SELF_VERSION_CHECK_ERROR
	MB_ICONEXCLAMATION, // SELF_INVALID_NETPATH
	MB_ICONEXCLAMATION, // SELF_NO_EXISTING_BRANCH
	MB_ICONEXCLAMATION, // SELF_NO_TARGET_VERSION
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
	"\t${desc_url}",
	// SELF_VERSION_CHECK_ERROR
	"An error occured while looking for an update. No update will be done\n"
	"\n"
	"An automatic update will be attempted on the next run, unless "
	"otherwise specified",
	// SELF_INVALID_NETPATH
	"An automatic update was attempted, but something is wrong with the "
	"infos retrieved from the server.\n"
	"\n"
	"The latest stable version can be found at\n"
	"\n"
	"\t${desc_url}",
	// SELF_NO_EXISTING_BRANCH
	"The server isn't able to retrieve infos relative to this build, thus "
	"no update will be done\n"
	"\n"
	"The latest stable release can be found at\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"An automatic update will be attempted on the next run, unless "
	"otherwise specified",
	// SELF_NO_TARGET_VERSION
	"An update couldn't be found, since the server doesn't know which is "
	"the latest version for this build.\n"
	"\n"
	"The latest stable release can be found at\n"
	"\n"
	"\t${desc_url}\n"
	"\n"
	"An automatic update will be attempted on the next run, unless "
	"otherwise specified"
};

const char *self_sig_error =
	"\n"
	"We advise against downloading it from the originating website until "
	"this problem has been resolved.";
/// ----------------------

int update_notify_thcrap(void)
{
	const size_t SELF_MSG_SLOT = (size_t)self_body;
	self_result_t ret = SELF_NO_UPDATE;
	json_t *run_cfg = runconfig_get();
	
	json_t* global_cfg = globalconfig_get();
	bool skip_check_mbox;
	json_t* skip_check_mbox_object = json_object_get(global_cfg, "skip_check_mbox");
	if (skip_check_mbox_object) {
		skip_check_mbox = json_boolean_value(skip_check_mbox_object);
	}
	else {
		skip_check_mbox = false;
	}

	const char *thcrap_dir = json_object_get_string(run_cfg, "thcrap_dir");
	
	char *arc_fn = NULL;
	const char *self_msg = NULL;
	ret = self_update(thcrap_dir, &arc_fn);
	if (ret == SELF_NO_UPDATE) {
		return ret;
	}
	
	const char* self_header;
	const bool vcheck_error = (ret == SELF_VERSION_CHECK_ERROR || ret == SELF_NO_EXISTING_BRANCH || ret == SELF_NO_TARGET_VERSION);
	// Since we don't have the name of the newest version when this kind of values appear,
	// we don't need to add self_header_failure
	if (ret == SELF_OK || vcheck_error) {
		self_header = "";
	} else {
		self_header = self_header_failure;
	}
	
	strings_sprintf(SELF_MSG_SLOT,
		"%s%s",
		self_header,
		self_body[ret]
	);
	
	const char* thcrap_url = json_object_get_string(run_cfg, "thcrap_url");
	if(ret == SELF_NO_SIG || ret == SELF_SIG_FAIL) {
		strings_strcat(SELF_MSG_SLOT, self_sig_error);
	}
	strings_replace(SELF_MSG_SLOT, "${project}", PROJECT_NAME());
	strings_replace(SELF_MSG_SLOT, "${project_short}", PROJECT_NAME_SHORT());
	strings_replace(SELF_MSG_SLOT, "${build}", self_get_target_version());
	strings_replace(SELF_MSG_SLOT, "${thcrap_dir}", thcrap_dir);
	strings_replace(SELF_MSG_SLOT, "${desc_url}", thcrap_url);
	self_msg = strings_replace(SELF_MSG_SLOT, "${arc_fn}", arc_fn);

	// Write message
	if (skip_check_mbox) {
		log_print("---------------------------\n");
		log_printf("%s\n", self_msg);
		log_print("---------------------------\n");
	}
	else {
		log_mboxf(NULL, MB_OK | self_msg_type[ret], self_msg);
	}

	// Write new skip_check_mbox flag
	skip_check_mbox = vcheck_error;
	json_object_set_new(global_cfg, "skip_check_mbox", json_boolean(skip_check_mbox));
	
	SAFE_FREE(arc_fn);
	
	return ret;
}

