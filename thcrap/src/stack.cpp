/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Patch stack evaluators and information.
  */

#include "thcrap.h"
#include "vfs.h"
#include <algorithm>
#include <list>

static std::vector<patch_t> stack;

TH_CALLER_CLEANUP(chain_free)
static char **resolve_chain_default(const char *fn)
{
	if (!fn) {
		return nullptr;
	}
	char **chain = (char**)malloc(3 * sizeof(char *));
	chain[0] = strdup(fn);
	chain[1] = fn_for_build(fn);
	chain[2] = nullptr;
	return chain;
}

TH_CALLER_CLEANUP(chain_free)
static char **resolve_chain_game_default(const char *fn)
{
	char *fn_common = fn_for_game(fn);
	const char *fn_common_ptr = fn_common ? fn_common : fn;
	char **ret = resolve_chain(fn_common_ptr);
	free(fn_common);
	return ret;
}

static resolve_chain_t resolve_chain_function      = resolve_chain_default;
static resolve_chain_t resolve_chain_game_function = resolve_chain_game_default;

char **resolve_chain(const char *fn)
{
	return resolve_chain_function(fn);
}

void set_resolve_chain(resolve_chain_t function)
{
	resolve_chain_function = function;
}

char **resolve_chain_game(const char *fn)
{
	return resolve_chain_game_function(fn);
}

void set_resolve_chain_game(resolve_chain_t function)
{
	resolve_chain_game_function = function;
}

static size_t chain_get_size(char **chain)
{
	size_t size = 0;
	if (chain) {
		while (chain[size]) ++size;
	}
	return size;
}

void chain_free(char **chain)
{
	if (chain) {
		for (size_t i = 0; chain[i]; i++) {
			free(chain[i]);
		}
		free(chain);
	}
}

bool TH_FASTCALL stack_chain_iterate(stack_chain_iterate_t *sci, char **chain, sci_dir_t direction)
{
	if (sci->fn) {
		size_t cur_chain_step = sci->chain_step + direction;
		if (cur_chain_step == sci->chain_limit) {
			if ((sci->patch_step += direction) == sci->patch_limit) {
				return false;
			}
			sci->patch_info = &stack[sci->patch_step];
			cur_chain_step = sci->chain_reset;
		}
		sci->fn = chain[sci->chain_step = cur_chain_step];
		return true;
	}
	else {
		//Defining this as a macro instead of a bool generates efficient CMOVS
		#define is_reverse (bool)(direction < 0)
		{
			const size_t stack_size = stack.size();
			if unexpected(!stack_size) {
				// Failsafe for when there are no patches
				return false;
			}
			sci->patch_limit = (is_reverse ? -1 : stack_size);
			sci->patch_step =  (is_reverse ? stack_size : -1) + direction;
			sci->patch_info = &stack[sci->patch_step];
		}{
			const size_t chain_size = chain_get_size(chain);
			sci->chain_limit = (is_reverse ? -1 : chain_size);
			sci->chain_reset = (is_reverse ? chain_size : -1) + direction;
			sci->fn = chain[sci->chain_step = sci->chain_reset];
		}
		#undef is_reverse
		return true;
	}
}

json_t* stack_json_resolve_chain(char **chain, size_t *file_size)
{
	json_t *ret = NULL;
	
	size_t json_size = 0;

	for (size_t n = 0; chain[n]; n++) {
		const char *const fn = chain[n];
		size_t size = 0;
		json_t *json_new = jsonvfs_get(fn, &size);
		if (json_new) {
			ret = json_object_merge(ret, json_new);
			log_printf("\n+ vfs:%s", fn);
			json_size += size;
		}
	}

	stack_chain_iterate_t sci;
	sci.fn = NULL;
	while (stack_chain_iterate(&sci, chain, SCI_FORWARDS)) {
		json_size += patch_json_merge(&ret, sci.patch_info, sci.fn);
	}

	log_print(ret ? "\n" : "not found\n");
	if(file_size) {
		*file_size = json_size;
	}
	return ret;
}

json_t* stack_json_resolve(const char *fn, size_t *file_size)
{
	json_t *ret = NULL;
	if (char** chain = resolve_chain(fn)) {
		if (chain[0]) {
			log_printf("(JSON) Resolving %s... ", fn);
			ret = stack_json_resolve_chain(chain, file_size);
		}
		chain_free(chain);
	}
	return ret;
}

HANDLE stack_file_resolve_chain(char **chain)
{
	stack_chain_iterate_t sci;
	sci.fn = NULL;

	// Both the patch stack and the chain have to be traversed backwards: Later
	// patches take priority over earlier ones, and build-specific files are
	// preferred over generic ones.
	while(stack_chain_iterate(&sci, chain, SCI_BACKWARDS)) {
		auto ret = patch_file_stream(sci.patch_info, sci.fn);
		if(ret != INVALID_HANDLE_VALUE) {
			patch_print_fn(sci.patch_info, sci.fn);
			log_print("\n");
			return ret;
		}
	}
	log_print("not found\n");
	return INVALID_HANDLE_VALUE;
}

char* stack_fn_resolve_chain(char **chain)
{
	stack_chain_iterate_t sci;
	sci.fn = NULL;

	// Both the patch stack and the chain have to be traversed backwards: Later
	// patches take priority over earlier ones, and build-specific files are
	// preferred over generic ones.
	while (stack_chain_iterate(&sci, chain, SCI_BACKWARDS)) {
		char *fn = fn_for_patch(sci.patch_info, sci.fn);
		DWORD attr = GetFileAttributesU(fn);
		if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
			return fn;
		}
		free(fn);
	}
	return nullptr;
}

HANDLE stack_game_file_stream(const char *fn)
{
	HANDLE ret = INVALID_HANDLE_VALUE;
	if (char** chain = resolve_chain_game(fn)) {
		if (chain[0]) {
			log_printf("(Data) Resolving %s... ", chain[0]);
			ret = stack_file_resolve_chain(chain);
		}
		chain_free(chain);
	}
	return ret;
}

void* stack_game_file_resolve(const char *fn, size_t *file_size)
{
	HANDLE handle = stack_game_file_stream(fn);
	if (handle != INVALID_HANDLE_VALUE) {
		return file_stream_read(handle, file_size);
	}
	return NULL;
}

json_t* stack_game_json_resolve(const char *fn, size_t *file_size)
{
	char *full_fn = fn_for_game(fn);
	json_t *ret = stack_json_resolve(full_fn, file_size);
	free(full_fn);
	return ret;
}

void stack_show_missing(void)
{
	std::vector<const patch_t*> rem_arcs;

	for (const patch_t& patch : stack) {
		if(patch.archive && !PathFileExists(patch.archive)) {
			rem_arcs.push_back(&patch);
		}
	}

	if(rem_arcs.size() > 0) {
		std::string rem_arcs_str;

		for (const patch_t* it : rem_arcs) {
			rem_arcs_str += "\t"s + it->archive + '\n';
		}
		log_mboxf(NULL, MB_OK | MB_ICONEXCLAMATION,
			"Some patches in your configuration could not be found:\n"
			"\n"
			"%s"
			"\n"
			"Please reconfigure your patch stack - either by running the configuration tool, "
			"or by simply editing your run configuration file (%s).",
			rem_arcs_str.c_str(), runconfig_runcfg_fn_get()
		);
	}
}

void stack_show_motds(void)
{
	for (const patch_t& patch : stack) {
		patch_show_motd(&patch);
	}
}

extern "C" TH_EXPORT void motd_mod_post_init(void)
{
	stack_show_motds();
}
/// -----------------------------------------

void stack_add_patch_from_json(json_t *patch)
{
	if (const char* archive_path = json_object_get_string(patch, "archive")) {
		stack.push_back(patch_init(archive_path, patch, stack.size() + 1));
	}
}

void stack_add_patch(patch_t *patch)
{
	stack.push_back(*patch);
}

void stack_remove_patch(const char *patch_id)
{

	auto check = [patch_id](const patch_t& patch) {
		return strcmp(patch.id, patch_id) == 0;
	};

	std::vector<patch_t>::iterator patch = std::find_if(stack.begin(), stack.end(), check);
	patch_free(&*patch);
	stack.erase(patch);
}

size_t stack_get_size()
{
	return stack.size();
}

void stack_foreach(void(*callback)(const patch_t *path, void *userdata), void *userdata)
{
	for (const patch_t &patch : stack) {
		callback(&patch, userdata);
	}
}

void stack_foreach_cpp(std::function<void(const patch_t*)> callback)
{
	for (const patch_t &patch : stack) {
		callback(&patch);
	}
}

void stack_print()
{
	size_t i = 0;

	log_print("Patches in the stack: ");
	for (const patch_t& patch : stack) {
		if (i != 0) {
			log_print(", ");
		}
		log_print(patch.id);
		i++;
	}
	log_print("\n");

	for (const patch_t& patch : stack) {

		log_printf(
			"\n"
			"[%zu] %s:\n"
			"  archive: %s\n"
			"  title: %s\n"
			"  update: %s\n"
			, patch.level, patch.id
			, patch.archive
			, patch.title
			, BoolStr(patch.update)
		);

		bool print_ignores = patch.ignore != NULL;
		log_print(print_ignores ?
				   "  ignore:" :
				   "  ignore: ''"
		);
		if (print_ignores) {
			for (size_t i = 0; patch.ignore[i]; ++i) {
				log_printf(" '%s'", patch.ignore[i]);
			}
		}
		bool print_servers = patch.servers != NULL;
		log_print(print_servers ?
				  "\n  servers:" :
				  "\n  servers: ''"
		);
		if (print_servers) {
			for (size_t i = 0; patch.servers[i]; ++i) {
				log_printf(" '%s'", patch.servers[i]);
			}
		}
	}
	log_print("\n");
}

int stack_check_if_unneeded(const char *patch_id)
{
	const char *c_game = runconfig_game_get();
	std::string game = c_game ? c_game : ""s;

	const char *build = runconfig_build_get();

	// (No early return if we have no game name, since we want
	//  to slice out the patch in that case too.)

	auto patch_it = std::find_if(stack.begin(), stack.end(), [patch_id](const patch_t &patch) {
		return patch.id != nullptr && strcmp(patch.id, patch_id) == 0;
	});
	if (patch_it == stack.end()) {
		return -1;
	}
	patch_t& patch = *patch_it;

	int game_found = 0;
	if (!game.empty()) {

		// <game>.js
		std::string js = game + ".js";
		game_found |= patch_file_exists(&patch, js.c_str());

		// <game>/ (directory)
		game_found |= patch_file_exists(&patch, game.c_str());

		if (build && !game_found) {
			// <game>.<build>.js
			std::string js = game + "." + build + ".js";
			game_found |= patch_file_exists(&patch, js.c_str());
		}
	}
	return !game_found;
}

int stack_remove_if_unneeded(const char *patch_id)
{
	int ret = stack_check_if_unneeded(patch_id);
	if (ret) stack_remove_patch(patch_id);
	return ret;
}

void stack_free()
{
	for (patch_t &patch : stack) {
		patch_free(&patch);
	}
	stack.clear();
}
