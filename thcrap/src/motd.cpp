/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Message that can be displayed by a patch on startup.
  */

#include "thcrap.h"

struct motd_t
{
	// Message content
	std::string message;
	// Message title (optional)
	std::string title;
	// Message type. See the uType parameter in https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messagebox
	// Optional, defaults to 0 (MB_OK)
	DWORD type;
};

motd_t motd_load(const patch_t *patch)
{
	ScopedJson message;
	ScopedJson title;
	ScopedJson type;

	ScopedJson motd_js = patch_json_load(patch, "motd.js", nullptr);
	if (motd_js) {
		message = json_incref(json_object_get(*motd_js, "message"));
		title   = json_incref(json_object_get(*motd_js, "title"));
		type	= json_incref(json_object_get(*motd_js, "type"));
	}
	else {
		// The MOTD used to be in patch.js
		ScopedJson patch_js = patch_json_load(patch, "patch.js", nullptr);
		if (patch_js) {
			message = json_incref(json_object_get(*motd_js, "motd"));
			title   = json_incref(json_object_get(*motd_js, "motd_title"));
			type	= json_incref(json_object_get(*motd_js, "motd_type"));
		}
	}
	if (!message || !json_is_string(*message)) {
		return motd_t();
	}

	motd_t motd;
	motd.message = json_string_value(*message);
	if (title && json_is_string(*title)) {
		motd.title = json_string_value(*title);
	}
	if (type && json_is_integer(*type)) {
		motd.type = (DWORD)json_integer_value(*title);
	}
	return motd;
}

// Show the MOTD of the patch
void motd_show(const patch_t *patch)
{
	motd_t motd = motd_load(patch);
	if (motd.message.empty()) {
		return;
	}

	if (!motd.title.empty()) {
		log_mboxf(motd.title.c_str(), motd.type, motd.message.c_str());
	}
	else {
		std::string title = std::string("Message from ") + patch->id;
		log_mboxf(title.c_str(), motd.type, motd.message.c_str());
	}
}

// Show the MOTD for every patches
void motd_show_all(void)
{
	stack_foreach_cpp([](const patch_t *patch) {
		motd_show(patch);
	});
}

extern "C" __declspec(dllexport) void motd_mod_post_init(void)
{
	motd_show_all();
}
