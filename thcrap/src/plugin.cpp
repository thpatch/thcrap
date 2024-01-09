/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Plug-in and module handling
  */

#include "thcrap.h"
#include <algorithm>
#include <string_view>
#include <math.h>
#include <time.h>
#include <process.h>

#if TH_X86
extern "C" {
// None of these signatures are accurate,
// but it makes the names match so that
// the linker can find the functions
extern int64_t __cdecl _CIfmod(long double, long double);
extern long double __cdecl _CIexp(long double);
extern long double __cdecl _CIlog(long double);
extern long double __cdecl _CIlog10(long double);
extern long double __cdecl _CIpow(long double, long double);
extern long double __cdecl _CIsqrt(long double);
extern long double __cdecl _CIsin(long double);
extern long double __cdecl _CIcos(long double);
extern long double __cdecl _CItan(long double);
extern long double __cdecl _CIasin(long double);
extern long double __cdecl _CIacos(long double);
extern long double __cdecl _CIatan(long double);
extern long double __cdecl _CIatan2(long double, long double);
extern double __cdecl __libm_sse2_exp(double);
extern float __cdecl __libm_sse2_expf(float);
extern double __cdecl __libm_sse2_log(double);
extern float __cdecl __libm_sse2_logf(float);
extern double __cdecl __libm_sse2_log10(double);
extern float __cdecl __libm_sse2_log10f(float);
extern double __cdecl __libm_sse2_pow(double, double);
extern float __cdecl __libm_sse2_powf(float, float);
extern double __cdecl __libm_sse2_sin(double);
extern float __cdecl __libm_sse2_sinf(float);
extern double __cdecl __libm_sse2_cos(double);
extern float __cdecl __libm_sse2_cosf(float);
extern double __cdecl __libm_sse2_tan(double);
extern float __cdecl __libm_sse2_tanf(float);
extern double __cdecl __libm_sse2_asin(double);
extern float __cdecl __libm_sse2_asinf(float);
extern double __cdecl __libm_sse2_acos(double);
extern float __cdecl __libm_sse2_acosf(float);
extern double __cdecl __libm_sse2_atan(double);
extern float __cdecl __libm_sse2_atanf(float);
extern double __cdecl __libm_sse2_atan2(double);

extern int64_t __cdecl _ftol(long double);
extern int64_t __cdecl _ftol2(long double);
extern int32_t __cdecl _ftol2_sse(long double);

extern int64_t __cdecl _allmul(int64_t, int64_t);
extern int64_t __cdecl _alldiv(int64_t, int64_t);
extern int64_t __cdecl _allrem(int64_t, int64_t);
extern int64_t __cdecl _alldvrm(int64_t, int64_t);
extern uint64_t __cdecl _aulldiv(uint64_t, uint64_t);
extern uint64_t __cdecl _aullrem(uint64_t, uint64_t);
extern uint64_t __cdecl _aulldvrm(uint64_t, uint64_t);
extern int64_t __cdecl _allshl(int64_t, uint8_t);
extern int64_t __cdecl _allshr(int64_t, uint8_t);
extern uint64_t __cdecl _aullshr(uint64_t, uint8_t);
}
#endif

using math_1arg_ptr = double(__cdecl*)(double);
using math_2arg_ptr = double(__cdecl*)(double, double);
using math_3arg_ptr = double(__cdecl*)(double, double, double);
using math_remquo_ptr = double(__cdecl*)(double, double, int*);
using memchr_ptr = const void*(__cdecl*)(const void*, int, size_t);
using strchr_ptr = const char*(__cdecl*)(const char*, int);
using strstr_ptr = const char*(__cdecl*)(const char*, const char*);

// Pointer casting is used to select the C style overload
static std::unordered_map<std::string_view, uintptr_t> funcs = {
	// Allocation functions
	{ "th_malloc", (uintptr_t)&malloc },
	{ "th_calloc", (uintptr_t)&calloc },
	{ "th_realloc", (uintptr_t)&realloc },
	{ "th_free", (uintptr_t)&free },
	{ "th_msize", (uintptr_t)&_msize },
	{ "th_expand", (uintptr_t)&_expand },
	{ "th_aligned_malloc", (uintptr_t)&_aligned_malloc },
	{ "th_aligned_realloc", (uintptr_t)&_aligned_realloc },
	{ "th_aligned_free", (uintptr_t)&_aligned_free },
	{ "th_aligned_msize", (uintptr_t)&_aligned_msize },

	// Memory functions
	{ "th_memcpy", (uintptr_t)&memcpy },
	{ "th_memmove", (uintptr_t)&memmove },
	{ "th_memcmp", (uintptr_t)&memcmp },
	{ "th_memset", (uintptr_t)&memset },
	{ "th_memccpy", (uintptr_t)&memccpy },
	{ "th_memchr", (uintptr_t)static_cast<memchr_ptr>(&memchr) },
	{ "th_strdup", (uintptr_t)&strdup },
	{ "th_strndup", (uintptr_t)&strndup },
	{ "th_strdup_size", (uintptr_t)&strdup_size },

	// String functions
	{ "th_strcmp", (uintptr_t)&strcmp },
	{ "th_strncmp", (uintptr_t)&strncmp },
	{ "th_stricmp", (uintptr_t)&stricmp },
	{ "th_strnicmp", (uintptr_t)&strnicmp },
	{ "th_strcpy", (uintptr_t)&strcpy },
	{ "th_strncpy", (uintptr_t)&strncpy },
	{ "th_strcat", (uintptr_t)&strcat },
	{ "th_strncat", (uintptr_t)&strncat },
	{ "th_strlen", (uintptr_t)&strlen },
	{ "th_strnlen_s", (uintptr_t)&strnlen_s },
	{ "th_strchr", (uintptr_t)static_cast<strchr_ptr>(&strchr) },
	{ "th_strrchr", (uintptr_t)static_cast<strchr_ptr>(&strrchr) },
	{ "th_strstr", (uintptr_t)static_cast<strstr_ptr>(&strstr) },
	{ "th_strrev", (uintptr_t)&_strrev },

	// Formatting functions
	{ "th_sprintf", (uintptr_t)&sprintf },
	{ "th_vsprintf", (uintptr_t)&vsprintf },
	{ "th_snprintf", (uintptr_t)&snprintf },
	{ "th_vsnprintf", (uintptr_t)&vsnprintf },
	{ "th_sscanf", (uintptr_t)&sscanf },
	{ "th_vsscanf", (uintptr_t)&vsscanf },
	{ "th_strftime", (uintptr_t)&strftime },

	// Math functions
	{ "th_fabsf", (uintptr_t)&fabsf }, { "th_fabs", (uintptr_t)static_cast<math_1arg_ptr>(&fabs) },
	{ "th_fmodf", (uintptr_t)&fmodf }, { "th_fmod", (uintptr_t)static_cast<math_2arg_ptr>(&fmod) },
	{ "th_remainderf", (uintptr_t)&remainderf }, { "th_remainder", (uintptr_t)static_cast<math_2arg_ptr>(&remainder) },
	{ "th_remquof", (uintptr_t)&remquof }, { "th_remquo", (uintptr_t)static_cast<math_remquo_ptr>(&remquo) },
	{ "th_fmaf", (uintptr_t)&fmaf }, { "th_fma", (uintptr_t)static_cast<math_3arg_ptr>(&fma) },
	{ "th_fmaxf", (uintptr_t)&fmaxf }, { "th_fmax", (uintptr_t)static_cast<math_2arg_ptr>(&fmax) },
	{ "th_fminf", (uintptr_t)&fminf }, { "th_fmin", (uintptr_t)static_cast<math_2arg_ptr>(&fmin) },
	{ "th_fdimf", (uintptr_t)&fdimf }, { "th_fdim", (uintptr_t)static_cast<math_2arg_ptr>(&fdim) },
	{ "th_expf", (uintptr_t)&expf }, { "th_exp", (uintptr_t)static_cast<math_1arg_ptr>(&exp) },
	{ "th_logf", (uintptr_t)&logf }, { "th_log", (uintptr_t)static_cast<math_1arg_ptr>(&log) },
	{ "th_log10f", (uintptr_t)&log10f }, { "th_log10", (uintptr_t)static_cast<math_1arg_ptr>(&log10) },
	{ "th_log2f", (uintptr_t)&log2f }, { "th_log2", (uintptr_t)static_cast<math_1arg_ptr>(&log2) },
	{ "th_powf", (uintptr_t)&powf }, { "th_pow", (uintptr_t)static_cast<math_2arg_ptr>(&pow) },
	{ "th_sqrtf", (uintptr_t)&sqrtf }, { "th_sqrt", (uintptr_t)static_cast<math_1arg_ptr>(&sqrt) },
	{ "th_hypotf", (uintptr_t)&hypotf }, { "th_hypot", (uintptr_t)static_cast<math_2arg_ptr>(&hypot) },
	{ "th_sinf", (uintptr_t)&sinf }, { "th_sin", (uintptr_t)static_cast<math_1arg_ptr>(&sin) },
	{ "th_cosf", (uintptr_t)&cosf }, { "th_cos", (uintptr_t)static_cast<math_1arg_ptr>(&cos) },
	{ "th_tanf", (uintptr_t)&tanf }, { "th_tan", (uintptr_t)static_cast<math_1arg_ptr>(&tan) },
	{ "th_asinf", (uintptr_t)&asinf }, { "th_asin", (uintptr_t)static_cast<math_1arg_ptr>(&asin) },
	{ "th_acosf", (uintptr_t)&acosf }, { "th_acos", (uintptr_t)static_cast<math_1arg_ptr>(&acos) },
	{ "th_atanf", (uintptr_t)&atanf }, { "th_atan", (uintptr_t)static_cast<math_1arg_ptr>(&atan) },
	{ "th_atan2f", (uintptr_t)&atan2f }, { "th_atan2", (uintptr_t)static_cast<math_2arg_ptr>(&atan2) },
	{ "th_ceilf", (uintptr_t)&ceilf }, { "th_ceil", (uintptr_t)static_cast<math_1arg_ptr>(&ceil) },
	{ "th_floorf", (uintptr_t)&floorf }, { "th_floor", (uintptr_t)static_cast<math_1arg_ptr>(&floor) },
	{ "th_truncf", (uintptr_t)&truncf }, { "th_trunc", (uintptr_t)static_cast<math_1arg_ptr>(&trunc) },
	{ "th_roundf", (uintptr_t)&roundf }, { "th_round", (uintptr_t)static_cast<math_1arg_ptr>(&round) },
	{ "th_nearbyintf", (uintptr_t)&nearbyintf }, { "th_nearbyint", (uintptr_t)static_cast<math_1arg_ptr>(&nearbyint) },

#if TH_X86
	// Various compiler intrinsics
	// to help with porting code
	{ "th_ftol", (uintptr_t)&_ftol }, { "th_ftol2", (uintptr_t)&_ftol2 }, { "th_ftol2_sse", (uintptr_t)&_ftol2_sse },
	{ "th_CIfmod", (uintptr_t)&_CIfmod },
	{ "th_CIexp", (uintptr_t)&_CIexp }, { "th_exp_sse2", (uintptr_t)&__libm_sse2_exp }, { "th_expf_sse2", (uintptr_t)&__libm_sse2_expf },
	{ "th_CIlog", (uintptr_t)&_CIlog }, { "th_log_sse2", (uintptr_t)&__libm_sse2_log }, { "th_logf_sse2", (uintptr_t)&__libm_sse2_logf },
	{ "th_CIlog10", (uintptr_t)&_CIlog10 }, { "th_log10_sse2", (uintptr_t)&__libm_sse2_log10 }, { "th_log10f_sse2", (uintptr_t)&__libm_sse2_log10f },
	{ "th_CIpow", (uintptr_t)&_CIpow }, { "th_pow_sse2", (uintptr_t)&__libm_sse2_pow }, { "th_powf_sse2", (uintptr_t)&__libm_sse2_powf },
	{ "th_CIsqrt", (uintptr_t)&_CIsqrt },
	{ "th_CIsin", (uintptr_t)&_CIsin }, { "th_sin_sse2", (uintptr_t)&__libm_sse2_sin }, { "th_sinf_sse2", (uintptr_t)&__libm_sse2_sinf },
	{ "th_CIcos", (uintptr_t)&_CIcos }, { "th_cos_sse2", (uintptr_t)&__libm_sse2_cos }, { "th_cosf_sse2", (uintptr_t)&__libm_sse2_cosf },
	{ "th_CItan", (uintptr_t)&_CItan }, { "th_tan_sse2", (uintptr_t)&__libm_sse2_tan }, { "th_tanf_sse2", (uintptr_t)&__libm_sse2_tanf },
	{ "th_CIasin", (uintptr_t)&_CIasin }, { "th_asin_sse2", (uintptr_t)&__libm_sse2_asin }, { "th_asinf_sse2", (uintptr_t)&__libm_sse2_asinf },
	{ "th_CIacos", (uintptr_t)&_CIacos }, { "th_acos_sse2", (uintptr_t)&__libm_sse2_acos }, { "th_acosf_sse2", (uintptr_t)&__libm_sse2_acosf },
	{ "th_CIatan", (uintptr_t)&_CIatan }, { "th_atan_sse2", (uintptr_t)&__libm_sse2_atan }, { "th_atanf_sse2", (uintptr_t)&__libm_sse2_atanf },
	{ "th_CIatan2", (uintptr_t)&_CIatan2 }, { "th_atan2_sse2", (uintptr_t)&__libm_sse2_atan2 },

	// 64 bit integer helpers
	{ "th_allmul", (uintptr_t)&_allmul },
	{ "th_alldiv", (uintptr_t)&_alldiv }, { "th_allrem", (uintptr_t)&_allrem }, { "th_alldvrm", (uintptr_t)&_alldvrm },
	{ "th_aulldiv", (uintptr_t)&_aulldiv }, { "th_aullrem", (uintptr_t)&_aullrem }, { "th_aulldvrm", (uintptr_t)&_aulldvrm },
	{ "th_allshl", (uintptr_t)&_allshl }, { "th_allshr", (uintptr_t)&_allshr }, { "th_aullshr", (uintptr_t)&_aullshr },
#endif

	// Utility functions
	{ "th_qsort", (uintptr_t)&qsort },
	{ "th_bsearch", (uintptr_t)&bsearch },
	{ "th_rand_s", (uintptr_t)&rand_s },

	// TODO: Get feedback about which thread functions are good
	// Thread functions
	//{ "th_beginthread", (uintptr_t)&_beginthread },
	//{ "th_beginthreadex", (uintptr_t)&_beginthreadex },
	//{ "th_endthread", (uintptr_t)&_endthread },
	//{ "th_endthreadex", (uintptr_t)&_endthreadex },

	// Windows functions
	{ "th_Sleep", (uintptr_t)&Sleep },
	{ "th_GetLastError", (uintptr_t)&GetLastError },
	{ "th_GetProcAddress", (uintptr_t)&GetProcAddress },
	{ "th_GetModuleHandleA", (uintptr_t)&GetModuleHandleA },
	{ "th_GetModuleHandleW", (uintptr_t)&GetModuleHandleW },
	//{ "th_WaitForSingleObject", (uintptr_t)&WaitForSingleObject },
	{ "th_QueryPerformanceCounter", (uintptr_t)&QueryPerformanceCounter },
	{ "th_QueryPerformanceFrequency", (uintptr_t)&QueryPerformanceFrequency },
};
static mod_funcs_t mod_funcs = {};
static mod_funcs_t patch_funcs = {};
static std::vector<HMODULE> plugins;

TH_EXPORT void TH_CDECL export_jansson_funcs_mod_init(void*) {
	HMODULE jansson_handle = GetModuleHandleA("jansson" DEBUG_OR_RELEASE ".dll");
	if (exported_func_t* jansson_funcs = GetExportedFunctions(jansson_handle)) {
		for (size_t i = 0; jansson_funcs[i].func != 0 && jansson_funcs[i].name != nullptr; i++) {
			funcs[jansson_funcs[i].name] = jansson_funcs[i].func;
		}
		free(jansson_funcs);
	}
}

uintptr_t func_get(const char *name)
{
	auto existing = funcs.find(name);
	if (existing != funcs.end()) {
		return existing->second;
	}
	return NULL;
}

uintptr_t func_get_len(const char *name, size_t name_len)
{
	auto existing = funcs.find({ name, name_len });
	if (existing != funcs.end()) {
		return existing->second;
	}
	return NULL;
}

int func_add(const char *name, uintptr_t addr) {
	auto existing = funcs.find(name);
	if (existing == funcs.end()) {
		funcs[strdup(name)] = addr;
		return 0;
	}
	else {
		log_printf("Overwriting function/codecave %s\n", name);
		existing->second = addr;
		return 1;
	}
}

bool func_remove(const char *name) {
	auto former_func = funcs.extract(name);
	const bool func_removed = !former_func.empty();
	if (func_removed) {
		free((void*)former_func.key().data());
	}
	return func_removed;
}

int patch_func_init(exported_func_t *funcs)
{
	if (funcs) {
		mod_funcs_t patch_funcs_new;
		patch_funcs_new.build(funcs, "_patch_");
		patch_funcs_new.run("init", NULL);
		patch_funcs.merge(patch_funcs_new);
	}
	return 0;
}

int plugin_init(HMODULE hMod)
{
	if(exported_func_t* funcs_new = GetExportedFunctions(hMod)) {
		mod_funcs_t mod_funcs_new;
		mod_funcs_new.build(funcs_new, "_mod_");
		mod_funcs_new.run("init", NULL);
		mod_funcs_new.run("detour", NULL);
		mod_funcs.merge(mod_funcs_new);
		for (size_t i = 0; funcs_new[i].func != 0 && funcs_new[i].name != nullptr; i++) {
			funcs[funcs_new[i].name] = funcs_new[i].func;
		}
		free(funcs_new);
	}	
	return 0;
}

void plugin_load(const char *const fn_abs, const char *fn)
{
	const size_t fn_len = strlen(fn);
	const size_t dbg_len = strlen("_d.dll");
	const bool is_debug_plugin = fn_len >= dbg_len && strcmp(fn + fn_len - dbg_len, "_d.dll") == 0;
#ifdef _DEBUG
	if (!is_debug_plugin) {
		log_printf("[Plugin] %s: release plugin ignored in debug mode (or this dll is not a plugin)\n", fn);
		return;
	}
#else
	if (is_debug_plugin) {
		log_printf("[Plugin] %s: debug plugin ignored in release mode\n", fn);
		return;
	}
#endif

	if (!CheckDLLFunction(fn_abs, "thcrap_plugin_init")) {
		log_printf("[Plugin] %s: not a plugin\n", fn);
		return;
	}

	if (HMODULE plugin = LoadLibraryExU(fn_abs, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) {
		switch (FARPROC func = GetProcAddress(plugin, "thcrap_plugin_init");
				(uintptr_t)func) {
			default:
				if (!func()) {
					log_printf("[Plugin] %s: initialized and active\n", fn);
					plugin_init(plugin);
					plugins.push_back(plugin);
					break;
				}
				log_printf("[Plugin] %s: not used for this game\n", fn);
				[[fallthrough]];
			case NULL:
				FreeLibrary(plugin);
		}
	}
	else {
		log_printf("[Plugin] Error loading %s: %s\n", fn_abs, lasterror_str());
	}
}

int plugins_load(const char *dir)
{
	// Apparently, successful self-updates can cause infinite loops?
	// This is safer anyway.
	std::vector<char*> dlls;

	const size_t dir_len = strlen(dir);
	char* const dll_path = strdup_size(dir, MAX_PATH + 8);
	strcat(dll_path, "\\*.dll");

	{
		WIN32_FIND_DATAA w32fd;
		HANDLE hFind = FindFirstFile(dll_path, &w32fd);
		if (hFind == INVALID_HANDLE_VALUE) {
			return 1;
		}
		do {
			// Necessary to avoid the nonsensical "Bad Image" message
			// box if you try to LoadLibrary() a 0-byte file.
			if (w32fd.nFileSizeLow | w32fd.nFileSizeHigh) {
				// Yes, "*.dll" means "*.dll*" in FindFirstFile.
				// https://blogs.msdn.microsoft.com/oldnewthing/20050720-16/?p=34883
				if (!stricmp(PathFindExtensionA(w32fd.cFileName), ".dll")) {
					dlls.push_back(strdup(w32fd.cFileName));
				}
			}
		} while (FindNextFile(hFind, &w32fd));
		FindClose(hFind);
	}
	
	// LoadLibraryEx() with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
	// requires an absolute path to not fail with GetLastError() == 87.
	char *const fn_start = &dll_path[dir_len + 1];
	for (char* dll : dlls) {
		strcpy(fn_start, dll);
		free(dll);
		plugin_load(dll_path, fn_start);
	}
	free(dll_path);
	return 0;
}

int plugins_close(void)
{
	log_print("Removing plug-ins...\n");
	for (HMODULE plugin_module : plugins) {
		FreeLibrary(plugin_module);
	}
	return 0;
}

inline void mod_funcs_t::build(exported_func_t* funcs, std::string_view infix) {
	if (funcs) {
		while (funcs->name && funcs->func) {
			if (const char* p = strstr(funcs->name, infix.data());
				p && *(p += infix.length()) != '\0') {
				(*this)[p].push_back((mod_call_type)funcs->func);
			}
			++funcs;
		}
	}
}

inline void mod_funcs_t::run(std::string_view suffix, void* param) {
	std::vector<mod_call_type>& func_array = (*this)[suffix];
	for (mod_call_type func : func_array) {
		func(param);
	}
}

void mod_func_run_all(const char *pattern, void *param)
{
	mod_funcs.run(pattern, param);
	patch_funcs.run(pattern, param);
}

void patch_func_run_all(const char *pattern, void *param)
{
	patch_funcs.run(pattern, param);
}

extern "C" int BP_patch_func_run_all(x86_reg_t *regs, json_t *bp_info) {
	if (const char* pattern = json_object_get_string(bp_info, "pattern")) {
		void* param = (void*)json_object_get_immediate(bp_info, regs, "param");
		patch_func_run_all(pattern, param);
	}
	return breakpoint_cave_exec_flag(bp_info);
}

inline void mod_funcs_t::remove(std::string_view suffix, mod_call_type func)
{
	std::vector<mod_call_type>& func_array = (*this)[suffix];
	auto elem = std::find_if(func_array.begin(), func_array.end(), [&func](mod_call_type func_in_array) {
		return func_in_array == func;
		});
	func_array.erase(elem);
}

void mod_func_remove(const char *pattern, mod_call_type func)
{
	mod_funcs.remove(pattern, func);
}

void patch_func_remove(const char *pattern, mod_call_type func)
{
	patch_funcs.remove(pattern, func);
}
