#include "thcrap.h"

#include <atomic>

#pragma section("lock",read,write,shared)
__declspec(allocate("lock"))
std::atomic<HANDLE> waiting_on_pid;
__declspec(allocate("lock"))
std::atomic<void*> address;

static HMODULE self_handle;

extern "C" {

void set_pid_to_wait_for(HANDLE id) {
	HANDLE dummy;
	do dummy = NULL;
	while (!waiting_on_pid.compare_exchange_strong(dummy, id));
	address = NULL;
}
void clear_pid_to_wait_for(HANDLE id) {
	waiting_on_pid.compare_exchange_strong(id, NULL);
}
void* wait_for_init_address_from_pid() {
	void* inject_address = NULL;
	while (address.compare_exchange_strong(inject_address, NULL));
	waiting_on_pid = NULL;
	return inject_address;
}

}

struct InjectData {
	size_t dll_name_offset;
	size_t func_name_offset;
	size_t load_library_flags;
	uint8_t param[0];
};

DWORD WINAPI inject_and_unload(void* param) {
	InjectData* data = (InjectData*)param;

	DWORD ret = 0;
	if (HMODULE injected_dll = LoadLibraryExW((LPCWSTR)((uintptr_t)param + data->dll_name_offset), NULL, (DWORD)data->load_library_flags)) {
		if (auto func = (LPTHREAD_START_ROUTINE)GetProcAddress(injected_dll, (LPCSTR)((uintptr_t)param + data->func_name_offset))) {
			ret = func(data->param);
		} else {
			FreeLibrary(injected_dll);
		}
	}
	FreeLibraryAndExitThread(self_handle, ret);
}

void write_address_for_this_pid(HANDLE self_id) {
	while (waiting_on_pid != self_id);
	address = &inject_and_unload;
}

BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpvReserved) {
	switch (ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			self_handle = hDll;
			if (lpvReserved == NULL) {
				write_address_for_this_pid((HANDLE)read_teb_member(ClientId.UniqueProcess));
			}
	}
	return TRUE;
}
