/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap/src/thcrap_update_wrapper.h>
#include "configure.h"
#include "search.h"

// Returns 1 if the selectors [a] and [b] refer to the same patch.
// The repository IDs can be NULLs to ignore them.
bool sel_match(const patch_desc_t& a, const patch_desc_t& b)
{
	bool absolute;
	bool repo_match;
	bool patch_match;

	absolute = a.repo_id && b.repo_id;

	if (absolute) {
		repo_match = (strcmp(a.repo_id, b.repo_id) == 0);
	}
	else {
		repo_match = true;
	}

	patch_match = (strcmp(a.patch_id, b.patch_id) == 0);

	return repo_match && patch_match;
}

bool IsSelected(const patch_sel_stack_t& sel_stack, const patch_desc_t& sel)
{
	for (const patch_desc_t& it : sel_stack) {
		if (sel_match(sel, it)) {
			return true;
		}
	}
	return false;
}

repo_t *find_repo_in_list(repo_t **repo_list, const char *repo_id)
{
	for (size_t i = 0; repo_list[i]; i++) {
		if (strcmp(repo_list[i]->id, repo_id) == 0) {
			return repo_list[i];
		}
	}
	return nullptr;
}

const repo_patch_t *find_patch_in_repo(const repo_t *repo, const char *patch_id)
{
	for (size_t i = 0; repo && repo->patches[i].patch_id; i++) {
		if (strcmp(repo->patches[i].patch_id, patch_id) == 0) {
			return &repo->patches[i];
		}
	}
	return nullptr;
}

// Locates a repository for [sel] in [repo_list], starting from [orig_repo_id].
std::string SearchPatch(repo_t **repo_list, const char *orig_repo_id, const patch_desc_t& sel)
{
	const repo_t *orig_repo = find_repo_in_list(repo_list, orig_repo_id);

	// Absolute dependency
	// In fact, just a check to see whether [sel] is available.
	if(sel.repo_id) {
		repo_t *remote_repo = find_repo_in_list(repo_list, sel.repo_id);
		if (remote_repo) {
			const repo_patch_t *patch = find_patch_in_repo(remote_repo, sel.patch_id);
			if (patch) {
				return remote_repo->id;
			}
		}
		return "";
	}

	// Relative dependency
	if (find_patch_in_repo(orig_repo, sel.patch_id)) {
		return orig_repo_id;
	}

	for (size_t i = 0; repo_list[i]; i++) {
		if (find_patch_in_repo(repo_list[i], sel.patch_id)) {
			return repo_list[i]->id;
		}
	}

	// Not found...
	return "";
}

// Adds a patch and, recursively, all of its required dependencies. These are
// resolved first on the repository the patch originated, then globally.
// Returns the number of missing dependencies.
int AddPatch(patch_sel_stack_t& sel_stack, repo_t **repo_list, patch_desc_t sel)
{
	int ret = 0;
	const repo_t *repo = find_repo_in_list(repo_list, sel.repo_id);
	patch_t patch_info = patch_bootstrap_wrapper(&sel, repo);
	patch_t patch_full = patch_init(patch_info.archive, nullptr, 0);
	patch_desc_t *dependencies = patch_full.dependencies;

	for (size_t i = 0; dependencies && dependencies[i].patch_id; i++) {
		patch_desc_t dep_sel = dependencies[i];

		if(!IsSelected(sel_stack, dep_sel)) {
			std::string target_repo = SearchPatch(repo_list, sel.repo_id, dep_sel);
			if(target_repo.empty()) {
				log_printf("ERROR: Dependency '%s/%s' of patch '%s' not met!\n", dep_sel.repo_id, dep_sel.patch_id, sel.patch_id);
				ret++;
			} else {
				free(dep_sel.repo_id);
				dep_sel.repo_id = strdup(target_repo.c_str());
				dependencies[i] = dep_sel;
				ret += AddPatch(sel_stack, repo_list, dep_sel);
			}
		}
	}

	stack_add_patch(&patch_full);
	sel_stack.push_back({strdup(sel.repo_id), strdup(sel.patch_id)});
	patch_free(&patch_info);
	return ret;
}

// Returns the number of patches removed.
int RemovePatch(patch_sel_stack_t& sel_stack, const char *patch_id)
{
	auto& match = [patch_id](const patch_desc_t& desc) {
		return strcmp(desc.patch_id, patch_id) == 0;
	};

	std::vector<patch_desc_t> deps;
	patch_sel_stack_t::iterator sel = std::find_if(sel_stack.begin(), sel_stack.end(), match);

	stack_foreach_cpp([&](const patch_t *patch) {
		for (size_t i = 0; patch->dependencies && patch->dependencies[i].patch_id; i++) {
			if (sel_match(patch->dependencies[i], *sel)) {
				for (patch_desc_t &stack_patch : sel_stack) {
					if (strcmp(patch->id, stack_patch.patch_id) == 0) {
						deps.push_back(stack_patch);
					}
				}
			}
		}
	});

	for (const patch_desc_t& it : deps) {
		RemovePatch(sel_stack, it.patch_id);
	}

	
	stack_remove_patch(patch_id);
	free(sel->patch_id);
	free(sel->repo_id);
	sel_stack.erase(sel);

	return 1;
}

// Prints all patches of [repo_js] that are not part of the [sel_stack],
// filling [list_order] with the order they appear in.
// Returns the final array size of [list_order].
int RepoPrintPatches(std::vector<patch_desc_t>& list_order, repo_t *repo, patch_sel_stack_t& sel_stack)
{
	size_t list_count = list_order.size();
	bool print_header = true;

	for (size_t i = 0; repo->patches[i].patch_id; i++) {
		repo_patch_t& patch = repo->patches[i];
		patch_desc_t sel = {
			repo->id,
			patch.patch_id
		};

		if(!IsSelected(sel_stack, sel)) {
			list_order.push_back(sel);

			if(print_header) {
                con_printf(
					"Patches from [%s] (%s):\n"
					"\t(Contact: %s)\n"
					"\n", repo->title, repo->id, repo->contact
				);
				print_header = false;
			}
			++list_count;
			con_clickable(std::to_wstring(list_count),
				to_utf16(stringf(" [%2d] %-20s %s", list_count, patch.patch_id, patch.title)));
		}
	}
	if(!print_header) {
        con_printf("\n");
	}
	return list_count;
}

int PrintSelStack(std::vector<patch_desc_t>& list_order, repo_t **repo_list, patch_sel_stack_t& sel_stack)
{
	size_t list_count = list_order.size();

	int width = console_width();
	VLA(char, hr, width + 1);
	memset(hr, '=', width);
	hr[width] = '\0';

	// After filling the entire width, the cursor will have already moved to
	// the next line, so we don't need to add a separate \n after the string.
    con_printf("%s",hr);

	if(sel_stack.empty()) {
		goto end;
	}
    con_printf(
		"\n"
		"Selected patches (in ascending order of priority):\n"
		"\n"
	);
	for (patch_desc_t& sel : sel_stack) {
		const repo_t *repo = find_repo_in_list(repo_list, sel.repo_id);
		const repo_patch_t *patch = find_patch_in_repo(repo, sel.patch_id);
		std::string full_id = std::string(sel.repo_id) + "/" + sel.patch_id;

		++list_count;
		con_clickable(std::to_wstring(list_count),
			to_utf16(stringf("  %2d. %-20s %s", list_count, full_id.c_str(), patch->title)));

		list_order.push_back(sel);
	}
    con_printf("\n%s", hr);

end:
	VLA_FREE(hr);
	return list_count;
}

size_t repo_sort_patches(repo_t *repo)
{
	size_t i = 0;
	while (repo->patches[i].patch_id) {
		i++;
	}

	std::sort(&repo->patches[0], &repo->patches[i], [](const repo_patch_t& a, const repo_patch_t& b) {
		return strcmp(a.patch_id, b.patch_id) < 0;
	});

	return i;
}

size_t get_stack_size(patch_sel_stack_t sel_stack)
{
	size_t i = 0;

	for (auto& it : sel_stack) {
		i++;
	}
	return i;
}

patch_sel_stack_t SelectPatchStack(repo_t **repo_list)
{
	patch_sel_stack_t sel_stack;
	// Total number of required lines in the console buffer
	size_t patches_count = 0;

	if(!repo_list[0]) {
		log_printf("\nNo repositories available -.-\n");
		goto end;
	}
	// Sort patches
	for (size_t i = 0; repo_list[i]; i++) {
		patches_count += repo_sort_patches(repo_list[i]);
	}
	if(patches_count == 0) {
		log_printf("\nNo patches available -.-\n");
		goto end;
	}

	stack_free();
	while(1) {
		size_t list_pick;
		size_t stack_size = get_stack_size(sel_stack);
		size_t stack_offset;
		size_t list_count = 0;

		std::vector<patch_desc_t> list_order;

		cls(0);
        console_prepare_prompt();

		log_printf("-----------------\n");
		log_printf("Selecting patches\n");
		log_printf("-----------------\n");
		log_printf(
			"\n"
			"\n"
		);

		for (size_t i = 0; repo_list[i]; i++) {
			list_count = RepoPrintPatches(list_order, repo_list[i], sel_stack);
		}
		list_count = PrintSelStack(list_order, repo_list, sel_stack);
        con_printf("\n");

		stack_offset = list_order.size() - stack_size;

		int still_picking = 1;
		do {
			list_pick = 0;
			if(stack_size) {
                con_printf(
					"(1 - %u to add more, %u - %u to remove from the stack, ENTER to confirm):\n",
					stack_offset, stack_offset + 1, list_count);
			} else {
				con_printf("Pick a patch (1 - %u):\n", list_count);
			}
			std::wstring buf = console_read();
			swscanf(buf.c_str(), L"%u", &list_pick);
			still_picking = buf[0] != '\0';
		} while((list_pick < 0 || list_pick > list_count) && still_picking);

		if(still_picking != 1) {
			if(!stack_size) {
                con_printf("\nPlease select at least one patch before continuing.\n");
				pause();
				continue;
			}
			break;
		}
		list_pick--;
		patch_desc_t sel = list_order[list_pick];
		if(list_pick < stack_offset) {
			int ret;
			log_printf("Resolving dependencies for %s/%s...\n", sel.repo_id, sel.patch_id);
			ret = AddPatch(sel_stack, repo_list, sel);
			if(ret) {
				log_printf(
					"\n"
					"%d unmet dependencies. This configuration will most likely not work correctly!\n",
					ret
				);
				pause();
			}
		} else {
			RemovePatch(sel_stack, sel.patch_id);
		}
	}
end:
	return sel_stack;
}
