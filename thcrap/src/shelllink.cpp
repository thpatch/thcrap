/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Shortcuts creation
  */

#include "thcrap.h"

#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>

typedef enum {
	LINK_FN = 3, // Slots 1 and 2 are used by thcrap_configure, that needs them for run_cfg_fn
	LINK_ARGS,
} shelllink_slot_t;

#define LINK_MACRO_EXPAND(macro) \
	macro(link_fn); \
	macro(target_cmd); \
	macro(target_args); \
	macro(work_path); \
	macro(icon_fn)

HRESULT CreateLink(
	const char *link_fn, const char *target_cmd, const char *target_args,
	const char *work_path, const char *icon_fn
)
{
	HRESULT hres;
	IShellLink* psl;

	// Get a pointer to the IShellLink interface. It is assumed that CoInitialize
	// has already been called.
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
	if (SUCCEEDED(hres)) {
		IPersistFile* ppf;

		LINK_MACRO_EXPAND(WCHAR_T_DEC);
		LINK_MACRO_EXPAND(WCHAR_T_CONV);

		// Set the path to the shortcut target and add the description.
		psl->SetPath(target_cmd_w);
		psl->SetArguments(target_args_w);
		psl->SetWorkingDirectory(work_path_w);
		psl->SetIconLocation(icon_fn_w, 0);

		// Query IShellLink for the IPersistFile interface, used for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

		if (SUCCEEDED(hres)) {
			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save(link_fn_w, FALSE);
			if (FAILED(hres)) {
				hres = GetLastError();
			}
			ppf->Release();
		}
		psl->Release();
		LINK_MACRO_EXPAND(WCHAR_T_FREE);
	}
	return hres;
}

int CreateShortcuts(const char *run_cfg_fn, games_js_entry *games)
{
	constexpr stringref_t loader_exe = "thcrap_loader" DEBUG_OR_RELEASE ".exe";
	int ret = 0;
	size_t self_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	VLA(char, self_fn, self_fn_len);

	GetModuleFileNameU(NULL, self_fn, self_fn_len);
	PathRemoveFileSpec(self_fn);
	PathAppendU(self_fn, "..");
	PathAddBackslashU(self_fn);

	// Yay, COM.
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	{
		VLA(char, self_path, self_fn_len + loader_exe.length());
		strcpy(self_path, self_fn);

		strcat(self_fn, "bin\\");
		strcat(self_fn, loader_exe.data());

		log_printf("Creating shortcuts");

		for (size_t i = 0; games[i].id; i++) {
			const char *link_fn = strings_sprintf(LINK_FN, "%s%s (%s).lnk", self_path, games[i].id, run_cfg_fn);
			const char *link_args = strings_sprintf(LINK_ARGS, "\"%s.js\" %s", run_cfg_fn, games[i].id);

			log_printf(".");

			if (CreateLink(link_fn, self_fn, link_args, self_path, games[i].path)) {
				log_printf(
					"\n"
					"Error writing to %s!\n"
					"You probably do not have the permission to write to the current directory,\n"
					"or the file itself is write-protected.\n",
					link_fn
				);
				ret = 1;
				break;
			}
		}
		VLA_FREE(self_path);
	}
	VLA_FREE(self_fn);
	CoUninitialize();
	return ret;
}
