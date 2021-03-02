#include "i18n_private.h"

HINSTANCE i18n_instance = nullptr;
bool i18n_is_enabled = false;

static DWORD i18n_tls = -1;
TLSBlock *i18n_tls_get()
{
	TLSBlock *tls = (TLSBlock*)TlsGetValue(i18n_tls);
	if (!tls) {
		tls = new TLSBlock();
		TlsSetValue(i18n_tls, (void*)tls);
	}
	return tls;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH: {
		i18n_instance = hinstDLL;
		i18n_tls = TlsAlloc();
		if (i18n_tls == TLS_OUT_OF_INDEXES) {
			return FALSE;
		}

		i18n_is_enabled = !i18n_init_path();
		break;
	}
	case DLL_PROCESS_DETACH:
		TlsFree(i18n_tls);
		break;
	case DLL_THREAD_DETACH:
		delete (TLSBlock*)TlsGetValue(i18n_tls);
		break;
	}
	return TRUE;
}
