/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  *
  * ----
  *
  * C wrapper around the IShellLink API (thanks Microsoft)
  */

extern "C"
{
	#include <win32_utf8.h>
	#include <shobjidl.h>
	#include <objbase.h>
	#include <objidl.h>
	#include <shlguid.h>

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
		if(SUCCEEDED(hres)) {
			IPersistFile* ppf;

			LINK_MACRO_EXPAND(WCHAR_T_DEC);
			LINK_MACRO_EXPAND(WCHAR_T_CONV_VLA);

			// Set the path to the shortcut target and add the description.
			psl->SetPath(target_cmd_w);
			psl->SetArguments(target_args_w);
			psl->SetWorkingDirectory(work_path_w);
			psl->SetIconLocation(icon_fn_w, 0);

			// Query IShellLink for the IPersistFile interface, used for saving the
			// shortcut in persistent storage.
			hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

			if(SUCCEEDED(hres)) {
				// Save the link by calling IPersistFile::Save.
				hres = ppf->Save(link_fn_w, FALSE);
				if(FAILED(hres)) {
					hres = GetLastError();
				}
				ppf->Release();
			}
			psl->Release();
			LINK_MACRO_EXPAND(WCHAR_T_FREE);
		}
		return hres;
	}
}
