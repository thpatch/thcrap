#include "i18n_private.h"

HINSTANCE g_instance = nullptr;
bool g_isEnabled = false;

DWORD g_tls = -1;
TLSProxy<const char*, 0> lastdomain_raw;
TLSProxy<std::string*, 1> lastdomain;
TLSProxy<I18nCache*, 2> lastcache;
void **tls_init() {
	void *tls = calloc(sizeof(void*), TLS_MAX_INDEX);
	TlsSetValue(g_tls, (void*)tls);
	lastdomain = new std::string();
	return (void**)tls;
}
static void tls_delete() {
	delete lastdomain;
	free(TlsGetValue(g_tls));
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH: {
		g_instance = hinstDLL;
		g_tls = TlsAlloc();
		if (g_tls == TLS_OUT_OF_INDEXES) {
			return FALSE;
		}

		g_isEnabled = !i18n_init_path();
		break;
	}
	case DLL_PROCESS_DETACH:
		TlsFree(g_tls);
		break;
	case DLL_THREAD_DETACH:
		tls_delete();
		break;
	}
	return TRUE;
}
