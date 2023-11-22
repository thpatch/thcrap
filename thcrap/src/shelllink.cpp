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
#include <filesystem>

HRESULT CreateLink(
	const std::filesystem::path& link_fn,
	const std::filesystem::path& target_cmd,
	const std::wstring& target_args,
	const std::filesystem::path& work_path,
	const std::filesystem::path& icon_fn
)
{
	HRESULT hres;
	IShellLink* psl;

	// Get a pointer to the IShellLink interface. It is assumed that CoInitialize
	// has already been called.
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
	if (SUCCEEDED(hres)) {
		IPersistFile* ppf;

		// Set the path to the shortcut target and add the description.
		psl->SetPath(target_cmd.wstring().c_str());
		psl->SetArguments(target_args.c_str());
		psl->SetWorkingDirectory(work_path.wstring().c_str());
		psl->SetIconLocation(icon_fn.wstring().c_str(), 0);

		// Query IShellLink for the IPersistFile interface, used for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

		if (SUCCEEDED(hres)) {
			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save(link_fn.wstring().c_str(), FALSE);
			if (FAILED(hres)) {
				hres = GetLastError();
			}
			ppf->Release();
		}
		psl->Release();
	}
	return hres;
}

void ReplaceStringTable(HANDLE hUpdate, std::vector<std::wstring> strings)
{
	if (strings.size() > 16) {
		throw std::runtime_error("ReplaceStringTable supports at most 16 strings");
	}

	std::vector<wchar_t> newStringTable;
	for (auto& str : strings) {
		size_t pos = newStringTable.size();
		newStringTable.resize(newStringTable.size() + 1 + str.length());
		newStringTable[pos] = static_cast<wchar_t>(str.length());
		std::copy(str.begin(), str.end(), newStringTable.begin() + pos + 1);
	}

	for (size_t i = strings.size(); i < 16; i++) {
		newStringTable.push_back(0);
	}

	UpdateResourceW(hUpdate, RT_STRING, MAKEINTRESOURCE(1),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), newStringTable.data(), newStringTable.size() * sizeof(wchar_t));
}

std::pair<LPVOID, DWORD> GetResource(HMODULE hMod, LPCWSTR resourceId, LPCWSTR resourceType)
{
	HRSRC hRes = FindResourceW(hMod, resourceId, resourceType);
	if (hRes == nullptr) {
	    return {};
	}

	HGLOBAL hResLoad = LoadResource(hMod, hRes);
	if (hResLoad == nullptr) {
	    return {};
	}

	LPVOID lpResLock = LockResource(hResLoad);
	if (lpResLock == nullptr) {
		return {};
	}

	DWORD size = SizeofResource(hMod, hRes);
	if (size == 0) {
		return {};
	}

	return std::make_pair(lpResLock, size);
}

#pragma pack(push, 1)
typedef struct GRPICONDIR
{
	WORD idReserved;
	WORD idType;
	WORD idCount;
} GRPICONDIR;

typedef struct GRPICONDIRENTRY
{
	BYTE  bWidth;
	BYTE  bHeight;
	BYTE  bColorCount;
	BYTE  bReserved;
	WORD  wPlanes;
	WORD  wBitCount;
	DWORD dwBytesInRes;
	WORD  nId;
} GRPICONDIRENTRY;
#pragma pack(pop)

static BOOL CALLBACK CopyIconGroupCallback(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam)
{
	LPWSTR *iconGroupId = (LPWSTR*)lParam;
	if (IS_INTRESOURCE(lpName)) {
		*iconGroupId = lpName;
	}
	else {
		*iconGroupId = wcsdup(lpName);
	}
	return FALSE;
}

LPWSTR GetIconGroupResourceId(HMODULE hModule)
{
	LPWSTR iconGroupId = nullptr;
	EnumResourceNamesW(hModule, RT_GROUP_ICON, CopyIconGroupCallback, (intptr_t)&iconGroupId);
	return iconGroupId;
}

void CopyIconGroup(HANDLE hUpdate, const std::filesystem::path& icon_path)
{
	HMODULE hIconExe = LoadLibraryExW(icon_path.wstring().c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
	if (hIconExe == nullptr) {
		return;
	}
	defer(FreeLibrary(hIconExe));

	LPWSTR iconGroupId = GetIconGroupResourceId(hIconExe);
	if (!iconGroupId) {
		return;
	}
	defer(if (!IS_INTRESOURCE(iconGroupId)) {
		free(iconGroupId);
	});

	auto [iconGroupData, iconGroupSize] = GetResource(hIconExe, iconGroupId, RT_GROUP_ICON);
	if (!iconGroupData || iconGroupSize < sizeof(GRPICONDIR)) {
		return;
	}

	const GRPICONDIR *groupIconDir = (GRPICONDIR*)iconGroupData;
	if (iconGroupSize < sizeof(GRPICONDIR) + groupIconDir->idCount * sizeof(GRPICONDIRENTRY)) {
		return;
	}
	const GRPICONDIRENTRY *groupIconDirEntries = (GRPICONDIRENTRY*)((uint8_t*)iconGroupData + sizeof(GRPICONDIR));

	UpdateResourceW(hUpdate, RT_GROUP_ICON, iconGroupId,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), iconGroupData, iconGroupSize);

	for (size_t i = 0; i < groupIconDir->idCount; i++) {
		auto [iconData, iconSize] = GetResource(hIconExe, MAKEINTRESOURCE(groupIconDirEntries[i].nId), RT_ICON);
		if (iconData && iconSize) {
			UpdateResourceW(hUpdate, RT_ICON, MAKEINTRESOURCE(groupIconDirEntries[i].nId),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), iconData, iconSize);
		}
	}
}

bool CreateWrapper(
	const std::filesystem::path& link_path,
	const std::filesystem::path& thcrap_dir,
	const std::wstring& loader_exe,
	const std::wstring& target_args,
	const std::filesystem::path& icon_path,
	ShortcutsType shortcut_type
)
{
	auto wrapper_path = thcrap_dir / loader_exe;
	if (!CopyFileW(wrapper_path.wstring().c_str(), link_path.wstring().c_str(), FALSE)) {
		log_printf("Could not copy %s to %s (%u)\n", wrapper_path.string().c_str(),
		link_path.string().c_str(), GetLastError());
		return false;
	}

	HANDLE hUpdate = BeginUpdateResourceW(link_path.wstring().c_str(), FALSE);
	if (!hUpdate) {
		DeleteFileW(link_path.wstring().c_str());
		return false;
	}

	auto target_dir = thcrap_dir / "bin";
	if (shortcut_type == SHTYPE_WRAPPER_RELPATH) {
		auto link_dir = link_path;
		link_dir.remove_filename();
		target_dir = target_dir.lexically_proximate(link_dir);
	}
	ReplaceStringTable(hUpdate, {
		target_dir.wstring(),
		target_args,
		loader_exe
	});
	CopyIconGroup(hUpdate, icon_path);

	EndUpdateResourceW(hUpdate, FALSE /* Don't discard changes */);
	return true;
}

std::filesystem::path GetThcrapDir()
{
	size_t self_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	VLA(char, self_fn, self_fn_len);

	GetModuleFileNameU(NULL, self_fn, self_fn_len);

	auto thcrap_dir = std::filesystem::u8path(self_fn);
	thcrap_dir.remove_filename();
	thcrap_dir /= "..";
	thcrap_dir = thcrap_dir.lexically_normal();

	VLA_FREE(self_fn);
	return thcrap_dir;
}

std::filesystem::path get_link_dir(ShortcutsDestination destination, const std::filesystem::path& self_path)
{
	switch (destination)
	{
	default:
		// Default to thcrap dir
		[[fallthrough]];

	case SHDESTINATION_THCRAP_DIR:
		return self_path;

	case SHDESTINATION_DESKTOP: {
		wchar_t szPath[MAX_PATH];
		if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, szPath) != S_OK) {
			return "";
		}
		return szPath;
	}

	case SHDESTINATION_START_MENU: {
		wchar_t szPath[MAX_PATH];
		if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, szPath) != S_OK) {
			return "";
		}

		std::filesystem::path path = szPath;
		path /= "thcrap";
		if (!std::filesystem::is_directory(path)) {
			std::filesystem::create_directory(path);
		}
		return path;
	}

	case SHDESTINATION_GAMES_DIRECTORY:
		// Will be overwritten later
		return "";
	}
}

std::filesystem::path GetIconPath(const char *icon_path_, const char *game_id)
{
	auto icon_path = std::filesystem::u8path(icon_path_);

	if (icon_path.filename() != "vpatch.exe")
		return icon_path;

	auto new_path = icon_path;
	new_path.replace_filename(std::string(game_id) + ".exe");
	if (std::filesystem::is_regular_file(new_path))
		return new_path;

	// Special case - EoSD
	if (strcmp(game_id, "th06") == 0) {
		new_path = icon_path;
		new_path.replace_filename(L"東方紅魔郷.exe");
		if (std::filesystem::is_regular_file(new_path))
			return new_path;
	}

	return icon_path;
}

int CreateShortcuts(const char *run_cfg_fn, games_js_entry *games, ShortcutsDestination destination, ShortcutsType shortcut_type)
{
	LPCWSTR loader_exe = L"thcrap_loader" DEBUG_OR_RELEASE_W L".exe";
	auto thcrap_dir = GetThcrapDir();
	auto self_path = thcrap_dir / L"bin" / loader_exe;
	auto link_dir = get_link_dir(destination, thcrap_dir);
	int ret = 0;

	if (shortcut_type != SHTYPE_SHORTCUT &&
		shortcut_type != SHTYPE_WRAPPER_ABSPATH &&
		shortcut_type != SHTYPE_WRAPPER_RELPATH) {
		log_print("Error creating shortcuts: invalid parameter for shortcut_type. Please report this error to the developpers.\n");
		return 1;
	}

	// Yay, COM.
	HRESULT com_init_succeded = E_FAIL;
	if (shortcut_type == SHTYPE_SHORTCUT) {
		com_init_succeded = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	}

	log_printf("Creating shortcuts");

	for (size_t i = 0; games[i].id; i++) {
		log_printf(".");

		if (destination == SHDESTINATION_GAMES_DIRECTORY) {
			link_dir = std::filesystem::absolute(std::filesystem::u8path(games[i].path)).remove_filename();
		}

		auto icon_path = GetIconPath(games[i].path, games[i].id);
		auto link_path = link_dir / (std::string(games[i].id) + " (" + run_cfg_fn + ").ext");
		std::string link_args = std::string("\"") + run_cfg_fn + ".js\" " + games[i].id;
		auto link_args_w = std::make_unique<wchar_t[]>(link_args.length() + 1);
		StringToUTF16(link_args_w.get(), link_args.c_str(), -1);

		if (shortcut_type == SHTYPE_SHORTCUT) {
			link_path.replace_extension("lnk");
			if (CreateLink(link_path, self_path, link_args_w.get(), thcrap_dir, icon_path)) {
				ret = 1;
			}
		}
		else if (shortcut_type == SHTYPE_WRAPPER_ABSPATH || shortcut_type == SHTYPE_WRAPPER_RELPATH) {
			link_path.replace_extension("exe");
			auto exe_args = std::wstring(loader_exe) + L" " + link_args_w.get();
			if (!CreateWrapper(link_path, thcrap_dir, loader_exe, exe_args, icon_path, shortcut_type)) {
				ret = 1;
			}
		}
		if (ret != 0) {
			log_printf(
				"\n"
				"Error writing to %s!\n"
				"You probably do not have the permission to write to its directory,\n"
				"or the file itself is write-protected.\n",
				link_path.string().c_str()
			);
			break;
		}
	}

	if (com_init_succeded == S_OK) {
		CoUninitialize();
	}
	return ret;
}
