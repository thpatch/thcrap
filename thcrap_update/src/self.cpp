/**
  * Touhou Community Reliant Automatic Patcher
  * Update module
  *
  * ----
  *
  * Digitally signed, automatic updates of thcrap itself.
  */

#include <thcrap.h>
#include <wincrypt.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include "update.h"
#include "server.h"
#include "self.h"

#define TEMP_FN_LEN 41

constexpr char SELF_SERVER[] = "https://thcrap.thpatch.net/";
constexpr char NETPATHS_FN[] = "thcrap_update.js";
constexpr char PREFIX_BACKUP[] = "thcrap_old_%s";
constexpr char PREFIX_NEW[] = "thcrap_new_";
constexpr char EXT_NEW[] = ".zip";

static char update_version[sizeof("0x20010101")];

/// Download notification window
/// ----------------------------
/*
 * Not meant to be particularly pretty, just to have at least something in
 * that regard for now. The move to Qt is going to blow up the download size
 * significantly, and while we do want to block for that download because of
 * the message box we pop up at the end, we don't want to block without any
 * visual indication to the user until an archive of multiple megabytes has
 * finished downloading.
 *
 * Written using raw calls to Ye Olde Win32 API mainly for practice reasons.
 * Windows 1.0-style dialog resource scripts are terrible if you want to do
 * i18n and automatic layout, being just a simple flat list of widgets
 * positioned using "dialog units" (which really are just a way of expressing
 * pixels independent of font sizes). Therefore, I think we should stop using
 * them even for the smallest of dialogs like this one. Just compare the
 * lengths we'll then have to go to in order to simply *translate* them
 * automatically (thcrap's dialog.c) with what we're doing here ourselves.
 *
 * The smartdlg naming scheme is because I'm thinking about writing some
 * Win32-specific GUIs in the future that use these same ideas – being easy
 * to translate and automatically adjusting their layout while still using as
 * much of Ye Olde Win32 API as possible.
 *
 * ~ Nmlgc
 */

#define RECT_EXPAND(rect) rect.left, rect.top, rect.right, rect.bottom

 // Custom message for updating progress bar from download thread
#define WM_UPDATE_PROGRESS (WM_USER + 1)

struct smartdlg_state_t {
	HANDLE event_created = CreateEvent(nullptr, true, false, nullptr);
	DWORD thread_id;
	HFONT hFont;
	HWND hWnd;
	HWND hProgress;
	std::mutex progress_mutex;
	size_t current_progress = 0;
	size_t total_size = 0;

	~smartdlg_state_t() {
		CloseHandle(event_created);
	}
};

LRESULT CALLBACK smartdlg_proc(
	HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam
)
{
	switch (uMsg) {
	case WM_UPDATE_PROGRESS: {
		auto state = (smartdlg_state_t*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (state && state->hProgress) {
			std::lock_guard<std::mutex> lock(state->progress_mutex);
			if (state->total_size > 0) {
				int pos = (int)((state->current_progress * 100) / state->total_size);
				SendMessage(state->hProgress, PBM_SETPOS, pos, 0);
			}
		}
		break;
	}
	case WM_CLOSE: // Yes, these are not handled by DefDlgProc().
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefDlgProcW(hWnd, uMsg, wParam, lParam);
}

void smartdlg_close(smartdlg_state_t* state)
{
	assert(state);
	if (state->hWnd) {
		SendMessageW(state->hWnd, WM_CLOSE, 0, 0);
	}
	if (state->hFont) {
		DeleteObject(state->hFont);
	}
}

DWORD WINAPI self_window_create_and_run(void* param)
{
	const char* TEXT =
		"A new build of the ${project} is being downloaded, please wait...";
	const size_t TEXT_SLOT = (size_t)TEXT;
	const char* text_final;
	auto state = (smartdlg_state_t*)param;

	assert(state);

	// Initialize common controls for progress bar
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icex);

	HMODULE hMod = GetModuleHandle(NULL);
	HDC hDC = GetDC(0);
	HWND label = NULL;
	DWORD wnd_style = WS_BORDER | WS_POPUP | WS_CAPTION;
	DWORD wnd_style_ex = WS_EX_TOPMOST | WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME;
	RECT screen_rect = {};
	RECT wnd_rect = {};
	RECT label_rect = {};
	LONG font_pad = 0;
	const int PROGRESS_HEIGHT = 20;

	NONCLIENTMETRICSW nc_metrics = {};
	nc_metrics.cbSize = sizeof(nc_metrics);

	if (SystemParametersInfoW(
		SPI_GETNONCLIENTMETRICS, sizeof(nc_metrics), &nc_metrics, 0
	)) {
		int height = nc_metrics.lfMessageFont.lfHeight;
		state->hFont = CreateFontIndirectW(&nc_metrics.lfMessageFont);
		font_pad = (height < 0 ? -height : height);
	}
	if (!SystemParametersInfoW(SPI_GETWORKAREA, sizeof(RECT), &screen_rect, 0)) {
		screen_rect.right = GetSystemMetrics(SM_CXSCREEN);
		screen_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
	}

	if (state->hFont) {
		SelectObject(hDC, state->hFont);
	}

	strings_strcat(TEXT_SLOT, TEXT);
	text_final = strings_replace(TEXT_SLOT, "${project}", PROJECT_NAME);

	DrawText(hDC, text_final, -1, &label_rect, DT_CALCRECT);

	wnd_rect = label_rect;
	label_rect.left += font_pad;
	label_rect.top += font_pad;
	wnd_rect.right += font_pad * 2;
	wnd_rect.bottom += font_pad * 4 + PROGRESS_HEIGHT;
	AdjustWindowRectEx(&wnd_rect, wnd_style, FALSE, wnd_style_ex);
	wnd_rect.right -= wnd_rect.left;
	wnd_rect.bottom -= wnd_rect.top;
	wnd_rect.left = (screen_rect.right / 2) - (wnd_rect.right / 2);
	wnd_rect.top = (screen_rect.bottom / 2) - (wnd_rect.bottom / 2);

	state->hWnd = CreateWindowExU(
		wnd_style_ex, (LPSTR)WC_DIALOG, PROJECT_NAME, wnd_style,
		RECT_EXPAND(wnd_rect), NULL, NULL, hMod, NULL
	);

	label = CreateWindowExU(
		WS_EX_NOPARENTNOTIFY, "Static", text_final, WS_CHILD | WS_VISIBLE,
		RECT_EXPAND(label_rect), state->hWnd, NULL, hMod, NULL
	);

	// Create progress bar
	RECT progress_rect = label_rect;
	progress_rect.top = label_rect.bottom + font_pad;
	progress_rect.bottom = progress_rect.top + PROGRESS_HEIGHT;

	state->hProgress = CreateWindowExW(
		0, PROGRESS_CLASSW, NULL,
		WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
		RECT_EXPAND(progress_rect),
		state->hWnd, NULL, hMod, NULL
	);

	// Set progress bar range
	SendMessage(state->hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

	SetWindowLongPtrW(state->hWnd, GWLP_WNDPROC, (LPARAM)smartdlg_proc);
	SetWindowLongPtrW(state->hWnd, GWLP_USERDATA, (LONG_PTR)state);

	if (state->hFont) {
		SendMessageW(state->hWnd, WM_SETFONT, (WPARAM)state->hFont, 0);
		SendMessageW(label, WM_SETFONT, (WPARAM)state->hFont, 0);
	}
	ShowWindow(state->hWnd, SW_SHOW);
	UpdateWindow(state->hWnd);
	SetEvent(state->event_created);

	// We must run this in the same thread anyway, so we might as well
	// combine creation and the message loop into the same function.

	MSG msg;
	BOOL msg_ret;

	while ((msg_ret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
		if (msg_ret != -1) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (DWORD)msg.wParam;
}
/// ----------------------------

// A superior GetTempFileName. Fills [fn] with [len] / 2 random bytes printed
// as their hexadecimal representation. Returns a pointer to the final \0 at
// the end of the file name.
static char* self_tempname(char* fn, size_t len, const char* prefix)
{
	char* p = fn;
	size_t prefix_len = prefix ? strlen(prefix) : 0;
	if (fn && len > 8 && prefix && prefix_len < len - 1) {
		HCRYPTPROV hCryptProv;
		size_t rnd_num = (len - 1) / 2;
		VLA(BYTE, rnd, rnd_num);
		size_t i = 0;

		ZeroMemory(fn, len);
		auto ret = W32_ERR_WRAP(CryptAcquireContext(
			&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT
		));
		if (!ret) {
			CryptGenRandom(hCryptProv, rnd_num, (BYTE*)rnd);
		}
		else {
			LARGE_INTEGER t;
			QueryPerformanceCounter(&t);
			t.HighPart ^= GetCurrentProcessId();
			for (i = 0; i < rnd_num / sizeof(t); i++) {
				memcpy(rnd + (i * sizeof(t)), &t, sizeof(t));
			}
		}
		for (i = 0; i < rnd_num; i++) {
			p += sprintf(p, "%02x", rnd[i]);
		}
		memcpy(fn, prefix, prefix_len);
		VLA_FREE(rnd);
		CryptReleaseContext(hCryptProv, 0);
	}
	return p;
}

static int self_pubkey_from_signer(PCCERT_CONTEXT* context)
{
	int ret = -1;
	HMODULE self_mod = GetModuleContaining((void*)(uintptr_t)self_pubkey_from_signer);
	HCERTSTORE hStore = NULL;
	HCRYPTMSG hMsg = NULL;
	DWORD msg_type = 0;
	DWORD param_len;
	DWORD signer_num;
	DWORD i;

	if (!context || !self_mod) {
		return -1;
	}
	{
		// CryptQueryObject() forces us to use the W version, but only
		// our U version can calculate the length of the string, so...
		uint32_t self_fn_len = GetModuleFileNameU(self_mod, NULL, 0) + 1;
		VLA(wchar_t, self_fn, self_fn_len * UTF8_MUL);
		VLA(char, self_fn_utf8, self_fn_len);
		GetModuleFileNameU(self_mod, self_fn_utf8, self_fn_len);
		GetModuleFileNameW(self_mod, self_fn, self_fn_len);

		log_printf(
			"Retrieving public key from the signer certificate of %s... ", self_fn_utf8
		);

		ret = W32_ERR_WRAP(CryptQueryObject(
			CERT_QUERY_OBJECT_FILE, self_fn,
			CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
			CERT_QUERY_FORMAT_FLAG_BINARY,
			0, &msg_type, NULL, NULL, &hStore, &hMsg, NULL
		));
		VLA_FREE(self_fn_utf8);
		VLA_FREE(self_fn);
	}
	if (ret) {
		goto end;
	}

	ret = W32_ERR_WRAP(CryptMsgGetParam(
		hMsg, CMSG_SIGNER_COUNT_PARAM, 0, &signer_num, &param_len
	));
	if (ret) {
		goto end;
	}
	for (i = 0; i < signer_num && !(*context); i++) {
		PCERT_INFO signer_info = NULL;

		ret = W32_ERR_WRAP(CryptMsgGetParam(
			hMsg, CMSG_SIGNER_INFO_PARAM, i, NULL, &param_len
		));
		if (ret) {
			continue;
		}
		signer_info = (PCERT_INFO)malloc(param_len);
		// MANDATORY
		ZeroMemory(signer_info, param_len);
		ret = W32_ERR_WRAP(CryptMsgGetParam(
			hMsg, CMSG_SIGNER_CERT_INFO_PARAM, i, signer_info, &param_len
		));
		if (ret) {
			continue;
		}
		*context = CertGetSubjectCertificateFromStore(
			hStore, msg_type, signer_info
		);
		ret = *context ? 0 : GetLastError();
		SAFE_FREE(signer_info);
	}
	log_print(ret ? "not found\n" : "OK\n");
end:
	CertCloseStore(hStore, 0);
	CryptMsgClose(hMsg);
	return ret;
}

// Prints at most [len] characters of the value of [hHash] into [buf]. Returns
// a pointer to the final \0 at the end of the printed hash, or [buf] in case
// the hash couldn't be written.
char* self_sprint_hash(char* buf, size_t len, HCRYPTHASH hHash)
{
	char* p = buf;
	DWORD hash_len = 0;
	DWORD hash_len_len = sizeof(hash_len);
	auto crypt_get_hash_param = [&](DWORD param, BYTE* buf, DWORD* len) {
		return W32_ERR_WRAP(CryptGetHashParam(hHash, param, buf, len, 0));
		};

	if (crypt_get_hash_param(HP_HASHSIZE, (BYTE*)&hash_len, &hash_len_len)) {
		return 0;
	}

	// Since we can't pass a nullptr as the length parameter of
	// CryptGetHashParam(), we might as well get the whole hash upfront and
	// merely truncate the string output.
	VLA(BYTE, hash_val, hash_len);
	if (!crypt_get_hash_param(HP_HASHVAL, hash_val, &hash_len)) {
		size_t bytes_in_suffix = (len - 1) / 2;
		size_t copy_len = MIN(bytes_in_suffix, hash_len) & ~1;
		for (size_t i = 0; i < copy_len; i++) {
			p += sprintf(p, "%02x", hash_val[i]);
		}
	}
	VLA_FREE(hash_val);
	return p;
}

static int self_verify_buffer(
	HCRYPTHASH* hHash,
	const void* file_buf,
	const size_t file_len,
	const json_t* sig,
	HCRYPTPROV hCryptProv,
	HCRYPTKEY hPubKey,
	ALG_ID hash_alg
)
{
	int ret = -1;
	const size_t sig_base64_len = json_string_length(sig);
	const char* sig_base64 = json_string_value(sig);
	DWORD i, j;
	DWORD sig_len = 0;
	BYTE* sig_buf = NULL;

	assert(hHash);
	if (!file_buf || !file_len || !sig_base64 || !sig_base64_len) {
		goto end;
	}
	ret = W32_ERR_WRAP(CryptStringToBinaryA(
		sig_base64, sig_base64_len, CRYPT_STRING_BASE64, NULL, &sig_len, NULL, NULL
	));
	if (!ret) {
		sig_buf = (BYTE*)malloc(sig_len);
		ret = W32_ERR_WRAP(CryptStringToBinaryA(
			sig_base64, sig_base64_len, CRYPT_STRING_BASE64, sig_buf, &sig_len, NULL, NULL
		));
	}
	if (ret) {
		log_print("invalid Base64 string\n");
		goto end;
	}
	// Reverse the signature...
	// (http://www.ruiandrebatista.com/windows-crypto-api-nightmares-rsa-signature-padding-and-byte-order-ramblings)
	for (i = 0, j = sig_len - 1; i < j; i++, j--) {
		BYTE t = sig_buf[i];
		sig_buf[i] = sig_buf[j];
		sig_buf[j] = t;
	}
	ret = W32_ERR_WRAP(CryptCreateHash(hCryptProv, hash_alg, 0, 0, hHash));
	if (ret) {
		log_print("couldn't create hash object\n");
		goto end;
	}
	ret = W32_ERR_WRAP(CryptHashData(*hHash, (BYTE*)file_buf, file_len, 0));
	if (ret) {
		log_print("couldn't hash the file data?!?\n");
		goto end;
	}

	ret = W32_ERR_WRAP(CryptVerifySignature(
		*hHash, sig_buf, sig_len, hPubKey, NULL, 0
	));

	log_print(ret ? "invalid\n" : "valid\n");
end:
	SAFE_FREE(sig_buf);
	return ret;
}

static ALG_ID self_alg_from_str(const char* hash)
{
	if (hash) {
		if (!stricmp(hash, "SHA1")) {
			return CALG_SHA1;
		}
		else if (!stricmp(hash, "SHA256")) {
			return CALG_SHA_256;
		}
	}
	return 0;
}

// We can't directly create and release the HCRYPTPROV in this function because
// calling CryptReleaseContext() invalidates *all* objects that were created
// from this CSP handle.
static self_result_t self_verify(
	HCRYPTPROV hCryptProv,
	HCRYPTHASH* hHash,
	const void* zip_buf,
	size_t zip_len,
	json_t* sig,
	PCCERT_CONTEXT context
)
{
	assert(hCryptProv);

	const json_t* sig_sig = json_object_get(sig, "sig");
	const char* sig_alg = json_object_get_string(sig, "alg");
	ALG_ID hash_alg = self_alg_from_str(sig_alg);
	HCRYPTKEY hPubKey = 0;

	if (!zip_buf || !zip_len || !json_is_string(sig_sig) || !context || !sig_alg) {
		return SELF_NO_SIG;
	}
	log_print("Verifying archive signature... ");
	if (!hash_alg) {
		log_func_printf("Unsupported hash algorithm ('%s')!\n", sig_alg);
		return SELF_NO_SIG;
	}
	if (W32_ERR_WRAP(CryptImportPublicKeyInfo(
		hCryptProv, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
		&context->pCertInfo->SubjectPublicKeyInfo, &hPubKey
	))) {
		log_func_print("Invalid public key!\n");
		return SELF_NO_PUBLIC_KEY;
	}
	return self_verify_buffer(
		hHash, zip_buf, zip_len, sig_sig, hCryptProv, hPubKey, hash_alg
	) ? SELF_SIG_FAIL : SELF_OK;
}

static int self_move_to_dir(const char* dst_dir, const char* fn)
{
	int ret;
	size_t full_fn_len = strlen(dst_dir) + 1 + strlen(fn) + 1;
	VLA(char, full_fn, full_fn_len);
	sprintf(full_fn, "%s/%s", dst_dir, fn);
	dir_create_for_fn(full_fn);
	ret = W32_ERR_WRAP(MoveFile(fn, full_fn));
	RemoveDirectory(fn);
	VLA_FREE(full_fn);
	return ret;
}

static self_result_t self_replace(zip_t* zip)
{
	self_result_t ret = SELF_REPLACE_ERROR;
	if (zip) {
		// + 1 for the underscore
		int prefix_backup_len = snprintf(NULL, 0, PREFIX_BACKUP, PROJECT_VERSION_STRING) + 1 + 1;
		VLA(char, prefix_backup, prefix_backup_len);
		char backup_dir[TEMP_FN_LEN];
		const char* fn;
		json_t* val;
		size_t i;

		sprintf(prefix_backup, PREFIX_BACKUP, PROJECT_VERSION_STRING);
		if (!PathFileExistsU(prefix_backup)) {
			strncpy(backup_dir, prefix_backup, sizeof(backup_dir));
		}
		else {
			strcat(prefix_backup, "_");
			self_tempname(backup_dir, sizeof(backup_dir), prefix_backup);
		}
		VLA_FREE(prefix_backup);

		// We don't error-check CreateDirectory(), as that might return
		// ERROR_ALREADY_EXISTS when directories are created recursively.
		// If this fails, writing should fail too.
		log_print("Replacing engine...\n");
		CreateDirectoryU(backup_dir, NULL);
		// TODO: Can these be changed to not pass a return value
		// to the foreach macros? It ends up invoking the function
		// multiple times.
		json_object_foreach(zip_list(zip), fn, val) {
			int local_ret = self_move_to_dir(backup_dir, fn);
			if (
				local_ret == ERROR_FILE_NOT_FOUND
				|| local_ret == ERROR_PATH_NOT_FOUND
				) {
				local_ret = 0;
			}
			if (local_ret || zip_file_unzip(zip, fn)) {
				goto end;
			}
		}
		json_array_foreach(zip_list_empty(zip), i, val) {
			DeleteFileU(json_string_value(val));
		}
		ret = SELF_OK;
	}
end:
	return ret;
}

self_result_t self_update(const char* thcrap_dir, char** arc_fn_ptr)
{
	self_result_t ret;
	std::string self_server;
	char arc_fn[TEMP_FN_LEN];
	zip_t* arc = NULL;
	PCCERT_CONTEXT context = NULL;
	HCRYPTPROV hCryptProv = 0;
	HCRYPTHASH hHash = 0;

	log_print("Checking for engine updates...\n");

	self_server = globalconfig_get_string("engine_update_url", SELF_SERVER);
	auto [netpaths, netpaths_status] = ServerCache::get().downloadJsonFile(self_server + NETPATHS_FN);
	if (!netpaths_status || !netpaths) {
		log_printf("%s%s: %s\n", self_server.c_str(), NETPATHS_FN, netpaths_status.toString().c_str());
		return SELF_VERSION_CHECK_ERROR;
	}

	const char* branch = PROJECT_BRANCH;
	json_t* branch_json = json_object_get(*netpaths, branch);
	if (!branch_json) {
		return SELF_NO_UPDATE;
	}

	uint32_t latest_version = (uint32_t)json_object_get_hex(branch_json, "version");
	if (!latest_version) {
		return SELF_NO_TARGET_VERSION;
	}

	if (latest_version <= PROJECT_VERSION) {
		return SELF_NO_UPDATE;
	}

	// Since we already have downloaded NETPATHS_FN and we are trying to resolve the
	// next exact version, we might as well get the right path immediately
	const char* version_key;
	const json_t* value;
	const char* netpath = nullptr;
	uint32_t target_version = latest_version;
	json_object_foreach_fast(branch_json, version_key, value) {
		errno = 0;
		uint32_t milestone = strtoul(version_key, NULL, 16);
		// Check if the string isn't an hex
		if (errno > 0 || !milestone) {
			continue;
		}

		if (milestone > PROJECT_VERSION && milestone < target_version) {
			netpath = json_string_value(value);
			target_version = milestone;
		}
	}

	if (target_version == latest_version) {
		netpath = json_object_get_string(branch_json, "latest");
	}

	// We know for sure which version we need
	str_hexdate_format(update_version, target_version);

	if (!netpath) {
		// If netpath is still null, branch_json is malformed.
		return SELF_INVALID_NETPATH;
	}

	// We are now trying an update
	smartdlg_state_t window;
	defer(smartdlg_close(&window));

	DWORD cur_dir_len = GetCurrentDirectoryU(0, NULL);

	VLA(char, cur_dir, cur_dir_len);
	defer(VLA_FREE(cur_dir));

	GetCurrentDirectoryU(cur_dir_len, cur_dir);

	SetCurrentDirectoryU(thcrap_dir);
	defer(SetCurrentDirectoryU(cur_dir));

	CreateThread(
		nullptr, 0, self_window_create_and_run, &window, 0, &window.thread_id
	);
	WaitForSingleObject(window.event_created, INFINITE);

	// Progress callback lambda
	auto progress_callback = [&window](const DownloadUrl&, size_t file_progress, size_t file_size) -> bool {
		{
			std::lock_guard<std::mutex> lock(window.progress_mutex);
			window.current_progress = file_progress;
			window.total_size = file_size;
		}
		// Notify UI thread to update progress bar
		if (window.hWnd) {
			PostMessage(window.hWnd, WM_UPDATE_PROGRESS, 0, 0);
		}
		return true;  // Continue download
		};

	auto [arc_dl, arc_dl_status] = ServerCache::get().downloadFile(
		self_server + netpath,
		progress_callback
	);
	if (!arc_dl_status || arc_dl.empty()) {
		log_printf("%s%s: %s\n", self_server.c_str(), netpath, arc_dl_status.toString().c_str());
		return SELF_SERVER_ERROR;
	}
	snprintf(arc_fn, TEMP_FN_LEN, "thcrap_update_download.zip");
	auto [sig, sig_status] = ServerCache::get().downloadJsonFile(self_server + netpath + ".sig");
	if (!sig_status || !sig) {
		log_printf("%s%s%s: %s\n", self_server.c_str(), netpath, ".sig", sig_status.toString().c_str());
		return SELF_NO_SIG;
	}

	if (W32_ERR_WRAP(CryptAcquireContext(
		&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT
	))) {
		return SELF_NO_PUBLIC_KEY;
	}
	defer(CryptReleaseContext(hCryptProv, 0));

	if (self_pubkey_from_signer(&context)) {
		return SELF_NO_PUBLIC_KEY;
	}
	defer(CertFreeCertificateContext(context));

	ret = self_verify(hCryptProv, &hHash, arc_dl.data(), arc_dl.size(), *sig, context);
	defer(CryptDestroyHash(hHash));
	if (ret != SELF_OK) {
		return ret;
	}

	if (file_write(arc_fn, arc_dl.data(), arc_dl.size())) {
		return SELF_DISK_ERROR;
	}
	if (arc_fn_ptr) {
		*arc_fn_ptr = (char*)malloc(TEMP_FN_LEN);
		memcpy(*arc_fn_ptr, arc_fn, TEMP_FN_LEN);
	}
	arc = zip_open(arc_fn);
	ret = self_replace(arc);
	zip_close(arc);
	if (ret != SELF_REPLACE_ERROR) {
		DeleteFile(arc_fn);
	}
	return ret;
}

const char* self_get_target_version()
{
	return update_version;
}
