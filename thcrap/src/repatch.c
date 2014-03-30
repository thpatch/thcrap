/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Hot-repatching.
  */

#include "thcrap.h"
#include "plugin.h"

/**
  * This module monitors all patches for external changes to their files, then
  * calls all "repatch" module functions if there have been any.
  *
  * Windows provides various ways to achieve this (see
  * http://qualapps.blogspot.com/2010/05/understanding-readdirectorychangesw.html).
  * Since we want to limit repatching to the files that actually did change,
  * change notifications are not an option. We also can't rely on a window
  * handle, which rules out SHChangeNotifyRegister(). NTFS Change Journals seem
  * to be tied to that specific file system, which alone makes this solution a
  * rather bad idea before even trying it.
  * This only leaves ReadDirectoryChangesW(). We use this function with the
  * I/O completion port strategy to monitor all patch directories using one
  * single port handle.
  *
  * However, that function still has one glaring design flaw: It forces us to
  * use a fixed-size buffer for what should essentially be a variable-size
  * queue. As a result, it is perfectly possible to lose file changes between
  * two calls if the change buffer was too small to hold everything that
  * happened during two calls to the function.
  *
  * Well, this means that we have to write some code around that function to
  * actually use it properly. This implementation uses two separate threads to
  * achieve what seems to be a pretty decent performance.
  *
  * • The "watcher" thread focuses exclusively on monitoring the changes by
  *   repeatedly calling ReadDirectoryChangesW() for all patch directories and
  *   buffering relevant changes in a performant way.
  *   This is *very* important, as we must spend as little time as possible on
  *   one filled buffer.
  *
  * • The separate "collector" thread waits for a fixed amount of time, then
  *   calls mod_func_run("repatch"), passing the current repatch buffer before
  *   flushing it.
  */

static HANDLE *dir_handles = NULL;
static int dir_handles_num = 0;
// Repatch buffer.
// A JSON object that receives the names of all changed files as the changes
// are detected.
static json_t *files_changed = NULL;
static CRITICAL_SECTION cs_changed = {0};
static HANDLE event_shutdown = NULL;
static HANDLE thread_watch = NULL;
static HANDLE thread_collect = NULL;
static volatile size_t changes_total = 0;

// Amount of milliseconds to wait before triggering the collector thread
#define COLLECT_THRESHOLD 1000

// 64K is more than 8 times as much as my system would crank out in a single
// watch loop while being bombarded with a cmd file creation FOR loop using
// names being close to MAX_PATH in length - so I guess it 'ought to be enough
// for anybody'™?
#define WATCH_BUFFER_SIZE 65536

void repatch_add(OVERLAPPED *ol)
{
	FILE_NOTIFY_INFORMATION *p = (FILE_NOTIFY_INFORMATION*)ol->Pointer;
	DWORD next_offset;
	do {
		size_t fn_utf16_len = (p->FileNameLength / 2);
		size_t fn_utf8_len = fn_utf16_len * UTF8_MUL;
		VLA(char, fn_utf8, fn_utf8_len);
		next_offset = p->NextEntryOffset;
		fn_utf8_len = WideCharToMultiByte(
			CP_UTF8, 0, p->FileName, fn_utf16_len, fn_utf8, fn_utf8_len, NULL, NULL
		);
		fn_utf8[fn_utf8_len] = 0;
		str_slash_normalize(fn_utf8);
		EnterCriticalSection(&cs_changed);
		// Marginally faster, and ensures correct counting of changes.
		if(!json_object_get(files_changed, fn_utf8)) {
			json_object_set_new(files_changed, fn_utf8, json_integer((json_int_t)ol->hEvent));
			InterlockedIncrement(&changes_total);
		}
		LeaveCriticalSection(&cs_changed);
		VLA_FREE(fn_utf8);
		if(next_offset) {
			p = (FILE_NOTIFY_INFORMATION*)ptr_dword_align(
				(BYTE*)p + sizeof(DWORD) * 3 + p->FileNameLength
			);
		}
	} while(next_offset);
}

DWORD WINAPI repatch_collector(void *param)
{
	while(WaitForSingleObject(event_shutdown, COLLECT_THRESHOLD) == WAIT_TIMEOUT) {
		if(json_object_size(files_changed) > 0) {
			json_t *files_changed_copy = NULL;
			log_printf("Repatching...\n");

			EnterCriticalSection(&cs_changed);
			files_changed_copy = json_copy(files_changed);
			json_object_clear(files_changed);
			LeaveCriticalSection(&cs_changed);
			mod_func_run("repatch", files_changed_copy);
			json_decref(files_changed_copy);
		}
	}
	return 0;
}

DWORD WINAPI repatch_watcher(void *param)
{
	DWORD ret_changes = TRUE;
	DWORD ret_queue = FALSE;
	DWORD ret_shutdown = WAIT_TIMEOUT;
	BYTE buf[WATCH_BUFFER_SIZE];
	int i;
	DWORD byte_ret_max = 0;

	OVERLAPPED ol_changes = {0};
	HANDLE hIOCompPort = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE, NULL, PROJECT_VERSION(), 0
	);
	for(i = 0; i < dir_handles_num; i++) {
		if(dir_handles[i] != INVALID_HANDLE_VALUE) {
			CreateIoCompletionPort(dir_handles[i], hIOCompPort, i, 0);
		}
	}
	while(!ret_queue && ret_shutdown == WAIT_TIMEOUT) {
		DWORD byte_ret = 0;
		DWORD key;
		OVERLAPPED *ol = NULL;

		// IMPORTANT. If we don't use a completion routine and this is not NULL,
		// ReadDirectoryChangesW() will think that we want to use the
		// GetOverlappedResult() notification strategy, which requires a valid
		// event handle. Since the ID we put in there is something else entirely,
		// the function *will fail* with ERROR_INVALID_HANDLE.
		ol_changes.hEvent = NULL;
		ol_changes.Pointer = buf;

		ret_queue = GetQueuedCompletionStatus(
			hIOCompPort, &byte_ret, &key, &ol, COLLECT_THRESHOLD
		) ? 0 : GetLastError();
		for(i = 0; i < dir_handles_num; i++) {
			ret_changes = ReadDirectoryChangesW(
				dir_handles[i], ol_changes.Pointer, sizeof(buf), TRUE,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
				&byte_ret, &ol_changes, NULL
			) ? 0 : GetLastError();
			if(byte_ret) {
				byte_ret_max = max(byte_ret_max, byte_ret);
			}
			if(!ret_queue && ol) {
				ol->hEvent = (HANDLE)key;
				repatch_add(ol);
			} else if(ret_queue == WAIT_TIMEOUT) {
				ret_queue = 0;
			}
		}
		ret_shutdown = WaitForSingleObject(event_shutdown, 0);
	}
	log_printf(
		"Shutting down repatch watcher thread.\n"
		"Statistics:\n"
		"----\n"
		"• Last error codes: %d (queue), %d (changes)\n"
		"• Total number of file changes parsed: %d\n"
		"• Maximum buffer fill state: %d/%d bytes\n"
		"----\n",
		ret_queue, ret_changes, changes_total, byte_ret_max, WATCH_BUFFER_SIZE
	);
	CloseHandle(hIOCompPort);
	return 0;
}

int repatch_mod_init(void)
{
	DWORD thread_id;
	const json_t *patches = json_object_get(runconfig_get(), "patches");
	size_t i;
	json_t *patch_info;
	size_t patches_num = json_array_size(patches);
	if(!patches_num) {
		return 1;
	}
	dir_handles = calloc(sizeof(HANDLE), patches_num);
	if(!dir_handles) {
		return 2;
	}
	json_array_foreach(patches, i, patch_info) {
		const char *archive = json_object_get_string(patch_info, "archive");
		HANDLE hDir = INVALID_HANDLE_VALUE;
		if(GetFileAttributes(archive) & FILE_ATTRIBUTE_DIRECTORY) {
			hDir = CreateFile(archive, FILE_LIST_DIRECTORY,
				FILE_SHARE_READ | FILE_SHARE_WRITE
				// Yes. This would otherwise prevent *files* in the
				// directory from being deleted or renamed, too.
				| FILE_SHARE_DELETE,
				NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
			);
		}
		dir_handles[dir_handles_num++] = hDir;
	}
	InitializeCriticalSection(&cs_changed);
	files_changed = json_object();
	event_shutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
	thread_watch = CreateThread(NULL, 0, repatch_watcher, NULL, 0, &thread_id);
	thread_collect = CreateThread(NULL, 0, repatch_collector, NULL, 0, &thread_id);
	return 0;
}

void repatch_mod_exit(void)
{
	SetEvent(event_shutdown);
	WaitForSingleObject(thread_watch, INFINITE);
	WaitForSingleObject(thread_collect, INFINITE);
	CloseHandle(thread_watch);
	CloseHandle(thread_collect);
	thread_watch = NULL;
	CloseHandle(event_shutdown);
	SAFE_FREE(dir_handles);
	event_shutdown = NULL;
	dir_handles_num = 0;
	files_changed = json_decref_safe(files_changed);
	DeleteCriticalSection(&cs_changed);
}
