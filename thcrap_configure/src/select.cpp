/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap/src/thcrap_update_wrapper.h>
#include "configure.h"
#include "search.h"

struct PatchCategory
{
	const char *description;
	int flags;
	std::function<bool (int flags)> filter;

	PatchCategory(const char *description, int flags)
		: description(description), flags(flags)
	{}
	PatchCategory(const char *description, std::function<bool (int flags)> filter)
		: description(description), flags(0), filter(filter)
	{}

	// Return true if the patch with these flags should be
	// displayed in this category, false otherwise.
	bool shouldDisplay(int flags)
	{
		if (this->filter) {
			return this->filter(flags);
		}
		else {
			return (flags & this->flags) && !(flags & PATCH_FLAG_HIDDEN);
		}
	}
};

std::vector<PatchCategory> categories = {
	{ "Most complete languages",          PATCH_FLAG_LANGUAGE },
	{ "All languages",                    [](int flags) { return flags & PATCH_FLAG_LANGUAGE; } },
	{ "Gameplay modifications",           PATCH_FLAG_GAMEPLAY },
	{ "Fanart sprites and texture packs", PATCH_FLAG_GRAPHICS },
	{ "Fanfictions",                      PATCH_FLAG_FANFICTION },
	{ "Alternative BGMs and arranges",    PATCH_FLAG_BGM },
	{ "All mods (gameplay patches, textures packs etc)", [] (int flags) { return (flags & (PATCH_FLAG_CORE | PATCH_FLAG_LANGUAGE)) == 0; } },
	{ "All patches",                      [](int) { return true; } },
};




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
	if (IsSelected(sel_stack, sel)) {
		return 0;
	}

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
	auto match = [patch_id](const patch_desc_t& desc) {
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

int PrettyPrintPatch(const char *patch, const char *title)
{
	con_printf("%-20s %s\n", patch, title);
	return 0;
}

// Prints all patches of [repo_js] that are part of the [category]
// filling [list_order] with the order they appear in.
// Returns the final array size of [list_order].
int RepoPrintPatches(PatchCategory& category, repo_t *repo, std::vector<patch_desc_t>& list_order)
{
	size_t list_count = list_order.size();
	bool print_header = true;

	for (size_t i = 0; repo->patches[i].patch_id; i++) {
		repo_patch_t& patch = repo->patches[i];
		patch_desc_t sel = {
			repo->id,
			patch.patch_id
		};

		if (category.shouldDisplay(patch.flags)) {
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
			con_clickable(list_count);
			con_printf(" [%2d] ", list_count);
			PrettyPrintPatch(patch.patch_id, patch.title);
		}
	}
	if(!print_header) {
		con_printf("\n");
	}
	return list_count;
}

int PrintSelStack(repo_t **repo_list, patch_sel_stack_t& sel_stack, size_t list_count = 0)
{
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
		"Selected patches:\n"
		"\n"
	);
	for (patch_desc_t& sel : sel_stack) {
		const repo_t *repo = find_repo_in_list(repo_list, sel.repo_id);
		const repo_patch_t *patch = find_patch_in_repo(repo, sel.patch_id);
		std::string full_id = std::string(sel.repo_id) + "/" + sel.patch_id;

		++list_count;
		con_clickable(list_count);
		con_printf("  %2d. ", list_count);
		PrettyPrintPatch(full_id.c_str(), patch->title);
	}
	con_printf("(you can select a patch to remove it from the list)\n");
	con_printf("\n\n");
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

static size_t pick_number(std::function<bool (size_t)> validate)
{
	char buf[16];
	char *endptr;
	long pick;

	while (true) {
		console_read(buf, sizeof(buf));
		pick = strtol(buf, &endptr, 10);
		if (
			*endptr == '\0'   // A number
			&& pick >= 0      // Positive number
			&& validate(pick) // The caller accept this number
		) {
			return pick;
		}
		log_printf("Invalid number.");
	}
}

PatchCategory *SelectCategory(repo_t **repo_list, patch_sel_stack_t& sel_stack)
{
	cls(0);
	console_prepare_prompt();

	con_printf(
		"-----------------\n"
		"Selecting patches\n"
		"-----------------\n"
		"\n"
		"\n"
	);
	PrintSelStack(repo_list, sel_stack, 100);

	con_printf("Select a category:\n");
	for (size_t i = 0; i < categories.size(); i++) {
		con_printf("[%d] %s\n", i + 1, categories[i].description);
	}
	if (!sel_stack.empty()) {
		con_printf("[0] Continue\n");
	}
	con_printf("\n");

	std::function<bool (size_t)> validate;
	if (!sel_stack.empty()) {
		con_printf("Pick a category (1 - %u), press 0 to continue, or select a patch to remove (101 - %u): ",
			categories.size(), 100 + sel_stack.size());
		validate = [&sel_stack](size_t pick) {
			if (pick == 0) return true;
			else if (pick >= 1 && pick < 1 + categories.size()) return true;
			else if (pick >= 101 && pick < 101 + sel_stack.size()) return true;
			else return false;
		};
	}
	else {
		con_printf("Pick a category (1 - %u): ", categories.size());
		validate = [](size_t pick) { return pick >= 1 && pick < 1 + categories.size(); };
	}

	size_t pick = pick_number(validate);
	if (pick == 0) {
		return nullptr;
	}
	else if (pick >= 1 && pick < 1 + categories.size()) {
		return &categories[pick - 1];
	}
	else if (pick >= 101 && pick < 101 + sel_stack.size()) {
		patch_desc_t& sel = sel_stack[pick - 101];
		RemovePatch(sel_stack, sel.patch_id);
		return SelectCategory(repo_list, sel_stack);
	}
	throw std::logic_error("Unreachable code");
}

patch_desc_t SelectPatchInCategory(repo_t **repo_list, PatchCategory& category)
{
	cls(0);
	console_prepare_prompt();

	con_printf(
		"-----------------\n"
		"Selecting patches\n"
		"-----------------\n"
		"\n"
		"\n"
	);
	con_printf("Select a patch:\n");

	std::vector<patch_desc_t> patches_in_category;
	for (size_t i = 0; repo_list[i]; i++) {
		RepoPrintPatches(category, repo_list[i], patches_in_category);
	}
	con_printf("\n");

	con_printf("Pick a patch (1 - %u): ", patches_in_category.size());
	size_t pick = pick_number([&patches_in_category](size_t pick) { return pick <= patches_in_category.size(); });
	if (pick == 0) {
		return {};
	}
	return patches_in_category[pick - 1];
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
		cls(0);
		console_prepare_prompt();

		PatchCategory* category = SelectCategory(repo_list, sel_stack);
		if (!category) {
			if (sel_stack.empty()) {
				con_printf("\nPlease select at least one patch before continuing.\n");
				pause();
				continue;
			}
			break;
		}

		patch_desc_t sel = SelectPatchInCategory(repo_list, *category);
		if (!sel.patch_id) {
			// Just loop back to the categories list
			continue;
		}

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
	}
end:
	return sel_stack;
}
