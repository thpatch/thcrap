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
	#include <thcrap.h>
	#include <shobjidl.h>
	#include <objbase.h>
	#include <objidl.h>
	#include <shlguid.h>

	#include "configure.h"

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

			// Fun.
			WCHAR_T_DEC(link_fn);
			WCHAR_T_DEC(target_cmd);
			WCHAR_T_DEC(target_args);
			WCHAR_T_DEC(work_path);
			WCHAR_T_DEC(icon_fn);
			link_fn_w = StringToUTF16_VLA(link_fn_w, link_fn, link_fn_len);
			target_cmd_w = StringToUTF16_VLA(target_cmd_w, target_cmd, target_cmd_len);
			target_args_w = StringToUTF16_VLA(target_args_w, target_args, target_args_len);
			work_path_w = StringToUTF16_VLA(work_path_w, work_path, work_path_len);
			icon_fn_w = StringToUTF16_VLA(icon_fn_w, icon_fn, icon_fn_len);

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
			VLA_FREE(link_fn_w);
			VLA_FREE(target_cmd_w);
			VLA_FREE(target_args_w);
			VLA_FREE(work_path_w);
			VLA_FREE(icon_fn_w);
		}
		return hres;
	}
}
