#include "i18n_private.h"

HINSTANCE g_instance = nullptr;
bool g_isEnabled = false;

static DWORD g_tls = -1;
TLSBlock *i18n_tls_get()
{
	TLSBlock *tls = (TLSBlock*)TlsGetValue(g_tls);
	if (!tls) {
		tls = new TLSBlock();
		TlsSetValue(g_tls, (void*)tls);
	}
	return tls;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
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
		delete (TLSBlock*)TlsGetValue(g_tls);
		break;
	}
	return TRUE;
}
