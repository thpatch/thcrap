/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Hot-repatching.
  */

#include "thcrap.h"
#include "repatch.h"
#include <thread>
#include <future>
#include <atomic>
#include <chrono>

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

/*
*  Note to future testers:
* 
*  Windows has "touch"-like functionality built into the COPY command
*  to update the timestamp of a file without changing the contents. The
*  syntax is like this:
*
*  copy /b "path/to/file"+,,
*
*  Normally the two commas would result in an obvious parsing error,
*  but it is supported as a special case to force touch operation.
*  See here: https://devblogs.microsoft.com/oldnewthing/20130710-00/?p=3843
*/

// ====================
// Utility Stuff
// ====================
// I'll freely admit that these exist purely
// because I'd rather make the compiler clean
// up the various handles/strings than figure
// out an efficient way of iterating them myself.

struct CloseAutoHandle {
	inline void operator()(HANDLE handle_to_close) {
		if (handle_to_close != INVALID_HANDLE_VALUE) CloseHandle(handle_to_close);
	}
};

// TODO: Supposedly unique_ptr is able to take an lvalue reference to
// a function instead of just a type, but for some reason this makes
// VS complain about a missing constructor...?
using AutoHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, CloseAutoHandle>;

// Allows for potential realloc calls if
// it ever becomes necessary in future,
// not to mention is slightly faster than
// delete anyway.
struct FreeAutoString {
	inline void operator()(char* string_to_free) {
		free(string_to_free);
	}
};

using AutoString = std::unique_ptr<char, FreeAutoString>;

// ====================
// Repatcher Watcher
// ====================

// This version of the repatch watcher can pass
// an array of C strings to the _mod_repatch functions
// instead of just a JSON object. Considering that
// those functions only ever read the keys of the
// object, this is a massive improvement.
#define UseJsonForOldCompatibilityTesting

// ReadDirectoryChangesW buffer size cannot be larger than 64KB
// in order to work over a network because of packet size limitations.
#define DIR_CHANGES_BUF_SIZE 65520

using namespace std::literals::chrono_literals;

class RepatchWatcher {

	// Struct for storing changes to an individual directory.
	struct DirectoryWatchData {

		static inline constexpr DWORD max_changes_buf_size = 65536;

		AutoHandle directory_handle;

		std::unique_ptr<OVERLAPPED> OverlappedKey;

		// ReadDirectoryChangesW *requires* the buffer to be at least DWORD aligned.
		std::aligned_storage_t<DIR_CHANGES_BUF_SIZE, alignof(DWORD)> changes_buffer;

		static_assert(sizeof changes_buffer < max_changes_buf_size, "Dir changes buffer too large! Size must be <64KB to work over a network.");

		// Theoretical maximum number of changes that could ever
		// be recorded in a single buffer. Obviously the actual
		// count will be much smaller since the filenames will
		// always be more than just a single null terminator, but
		// having some excess capacity to prevent reallocations
		// seems like a good idea.
		static inline constexpr size_t max_entries = sizeof changes_buffer / sizeof(FILE_NOTIFY_INFORMATION);

		// Queues an asynchronous directory watch for the contained
		// handle using the appropriate overlapped pointer.
		inline void QueueDirectoryWatch(void) {
			ReadDirectoryChangesW(
				this->directory_handle.get(), /*hDirectory*/
				&this->changes_buffer, /*lpBuffer*/
				sizeof this->changes_buffer, /*nBufferLength*/
				TRUE, /*bWatchSubtree*/
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, /*dwNotifyFilter*/
				NULL, /*lpBytesReturned*/
				this->OverlappedKey.get(), /*lpOverlapped*/
				NULL /*lpCompletionRoutine*/
			);
		}
	};

// Variables

	std::atomic_bool running = false;

	std::mutex changed_files_mutex;

	std::vector<AutoString> changed_files;

	std::unordered_map<LPOVERLAPPED, DirectoryWatchData> watched_directories;

	AutoHandle io_completion_handle;

	std::thread watch_thread;
	std::thread collect_thread;

	size_t total_changes = 0;

	static constexpr auto collect_sleep_time = 1s;
	static constexpr auto watch_sleep_time = 1s;

// Methods

	inline void AddChangesToList(FILE_NOTIFY_INFORMATION* p) {

		// Lock the changes list while items are added
		const std::lock_guard<std::mutex> no_touchy_my_change_list(this->changed_files_mutex);

		do {
			AutoString filename_utf8((char*)malloc(p->FileNameLength * (UTF8_MUL / 2) + 1));
			if (StringToUTF8(filename_utf8.get(), p->FileName, -1) != -1) {
				this->changed_files.emplace_back(std::move(filename_utf8));
			}
		} while (p->NextEntryOffset ? (char*&)p += p->NextEntryOffset, true : false);
	}

	/*
	* Normally when working with ReadDirectoryChangesW and completion
	* ports the arbitrary "completion key" that was originally passed
	* to CreateIoCompletionPort would be used for identifying which
	* directory handle a de-queued completion packet came from and
	* the OVERLAPPED pointer would contain no data relevant to change
	* tracking.
	*
	* Unfortunately, the documentation about OVERLAPPED structures is *very*
	* clear that a unique structure should be used for each simultaneous
	* operation. Because all of our calls to ReadDirectoryChangesW are
	* going to be happening concurrently, every handle will require
	* its own unique OVERLAPPED struct.
	*
	* However, if there already needs to be a unique OVERLAPPED struct
	* for each handle, why even bother creating/tracking completion
	* keys in the first place? Instead, 
	*/

	void WatchLoop(void) {
		union { // Declared as a union just in case these typedefs are weird;
			DWORD A;
			ULONG_PTR B;
		} idgaf;
		OVERLAPPED* CurrentOverlappedKey;
		while (this->running) {
			std::this_thread::sleep_for(watch_sleep_time);
			while (GetQueuedCompletionStatus(this->io_completion_handle.get(), &idgaf.A, &idgaf.B, &CurrentOverlappedKey, INFINITE)) {
				if (!CurrentOverlappedKey) {
					// Probably the shutdown event, so break the loop
					// to go check if the repatcher is still running.
					break;
				}
				DirectoryWatchData& ChangedDirData = this->watched_directories[CurrentOverlappedKey];
				this->AddChangesToList((FILE_NOTIFY_INFORMATION*)&ChangedDirData.changes_buffer);
				ChangedDirData.QueueDirectoryWatch();
			}
		}
	}

	inline void ApplyCollectedChanges(void) {

		// Lock the changes list until repatching is complete
		const std::lock_guard<std::mutex> no_its_my_change_list_now(this->changed_files_mutex);

		log_print("Repatching...\n");

		this->total_changes += this->changed_files.size();

#ifndef UseJsonForOldCompatibilityTesting
		this->changed_files.emplace_back(nullptr);
		mod_func_run_all("repatch", &this->changed_files[0]);
#else
		json_t* dummy_value = json_null();
		json_t* compat_test_json = json_object();
		for (auto& string : this->changed_files) {
			json_object_set_new_nocheck(compat_test_json, string.get(), dummy_value);
		}
		mod_func_run_all("repatch", compat_test_json);
		json_decref(compat_test_json);
#endif
		this->changed_files.clear();
	}

	void CollectLoop(void) {
		while (this->running) {
			std::this_thread::sleep_for(collect_sleep_time);
			if (!this->changed_files.empty()) {
				ApplyCollectedChanges();
			}
		}
	}

public:

	inline RepatchWatcher(void) :
		io_completion_handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1))
	{
		this->changed_files.reserve(DirectoryWatchData::max_entries);
	}

	inline void StopWatchLoop(void) {
		if (this->running == true) {
			// Signal the threads to stop
			this->running = false;

			// Wait until the threads have both terminated
			if (this->watch_thread.joinable()) {
				// Make sure the watcher thread receives a
				// message so that the "infinite" wait will stop.
				PostQueuedCompletionStatus(this->io_completion_handle.get(), 0, NULL, NULL);

				this->watch_thread.join();
			}
			if (this->collect_thread.joinable()) {
				this->collect_thread.join();
			}
		}
	}

	inline ~RepatchWatcher(void) {
		this->StopWatchLoop();
	}

	inline void StartWatchLoop(void) {
		if (this->running == false) {
			this->running = true;
			this->watch_thread = std::thread([this]() { this->WatchLoop(); });
			this->collect_thread = std::thread([this]() { this->CollectLoop(); });
		}
	}

	inline void AddPatchDirectory(const patch_t* patch) {

		// FILE_SHARE_DELETE is necessary to allow files in the
		// directory to be deleted or renamed externally.
		HANDLE patch_directory_handle = CreateFile(
			patch->archive, /*lpFileName*/
			FILE_LIST_DIRECTORY, /*dwDesiredAccess*/
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, /*dwShareMode*/
			NULL, /*lpSecurityAttributes*/
			OPEN_EXISTING, /*dwCreationDisposition*/
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, /*dwFlagsAndAttributes*/
			NULL /*hTemplateFile*/
		);

		if (patch_directory_handle == INVALID_HANDLE_VALUE) {
			// Could not open handle
			return;
		}

		// Verify the path even points to a directory.
		// Uses GetFileInformationByHandle instead of GetFileAttributes
		// to avoid converting the directory path to UTF16 twice.
		if (BY_HANDLE_FILE_INFORMATION file_info;
			GetFileInformationByHandle(patch_directory_handle, &file_info) &&
			file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			// Associate the new handle with the completion port
			// to receive notifications about
			CreateIoCompletionPort(patch_directory_handle, this->io_completion_handle.get(), NULL, 1);

			// make_unique automatically zero
			// initializes the OVERLAPPED struct
			auto NewOverlappedKey = std::make_unique<OVERLAPPED>();

			// TODO: Could this be simplified to somehow construct everything in place?
			// The only reason for even having the OverlappedKey member of DirectoryWatchData
			// is to properly extend the lifetime 
			DirectoryWatchData& NewWatch = this->watched_directories[NewOverlappedKey.get()];
			NewWatch.OverlappedKey = std::move(NewOverlappedKey);
			NewWatch.directory_handle.reset(patch_directory_handle);

			// Queue the initial directory watch to make windows setup
			// the relevant internal buffers.
			NewWatch.QueueDirectoryWatch();
		}
	}

	inline void AddAllPatchDirectories(void) {
		stack_foreach_cpp([this](const patch_t* patch) {
			this->AddPatchDirectory(patch);
		});
	}

	inline size_t TotalChanges() {
		return total_changes;
	}
};

RepatchWatcher repatcher;

void repatch_mod_init(void*) {
	repatcher.AddAllPatchDirectories();
	repatcher.StartWatchLoop();
}

// Logging can't be done in the destructor
void repatch_mod_exit(void*) {
	log_printf(
		"Shutting down repatch watcher thread.\n"
		"Statistics:\n"
		"----\n"
		"• Total number of file changes parsed: %d\n"
		"----\n"
		, repatcher.TotalChanges()
	);
}
