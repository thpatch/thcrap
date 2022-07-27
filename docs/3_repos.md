# Repositories
Repositories have already been described in the introduction.

The `repos` directory contains a directory per repository. Older versions of thcrap_configure used to create a directory for every repository found during the discovery (more on that later) because they wanted to store repo.js on disk, newer versions will keep the repo.js in RAM and write it only for selected patches.

Every repository contains a `repo.js` file describing the repository, and a directory for every selected patch. 
For example, you can have these files on disk:
```
repos
\ nmlgc
  - repo.js
  \ base_tsa
    - patch.js
    - Other files...
  \ base_tasofro
    - patch.js
    - Other files...
\ thpatch
  - repo.js
  \ lang_en
    - patch.js
    - Other files...
```

## repo.js
repo.js decribes a repository and all its patches. Here is an example repo.js file:
```
{
	"id": "nmlgc",
	"title": "Nmlgc's patch repository",
	"contact": "thcrap@nmlgc.net",
	"patches": {
		"aero": "Enable Aero compositing"
	},
	"patchdata": {
		"aero": {
			"flags": "gameplay",
			"games": [ "th125", "th128", "th13", "th14", "th143", "th15", "th16", "th165", "th17" ],
			"dependencies": [
				"base_tsa",
				"thpatch/lang_en"
			]
		}
	},
	"servers": [
		"https://mirrors.thpatch.net/nmlgc/"
	],
	"neighbors": [
		"https://srv.thpatch.net"
	]
}
```
### id
The id of the repo. It must be unique. Also, some parts of thcrap may assume that the name of the repo directory is the repo ID.

### title
A short description of the repository, displayed aside the id in thcrap_configure.

### contact
A way to contact the repository owner. It can be an e-mail address, a Discord tag, an URL... It is treated as plain text.

### patches
The list of patches provided by this repo, with a short description for each patch. This list determines which patches will be shown in thcrap_configure.

### patchdata (not yet implemented, see [this branch](https://github.com/thpatch/thcrap/compare/master...patch_js_removal))
More information about the patches. This information isn't in the patches section for backward compatibility. 
Each entry adds more information about a patch. 
Some of this information is currently specified in the patch.js file, but providing it at the repo level will allow us to use them in thcrap_configure without downloading a lot of new files during patch selection.

#### flags
A flag or a set of flags used to put your patch in a category. With these, the user can choose to see only the language or gameplay patches in thcrap_configure, and your patch will be listed only if it has the relevant flag(s).
The supported flags are:
- core: adds a core feature used by another patch. Usually hidden, because the user don't want to select it, they want to select an useful patch and have the core patch pulled as a dependency.
- language: translation patches.
- gameplay: patches that changes the gameplay, like cheats, speed modifiers or new patterns.
- graphics: patches that changes the graphics, for example the sprites.
- fanfiction: patches that changes the dialogs, making a new story inside the game.
- hidden: Usually not the only flag. Tells that the patch should be hidden from the default list. Used for patches that have been replaced by other patches, or for inactive and mostly incomplete translations.

#### games
List of games supported by the patch.

#### dependencies
List of dependencies for the patch. When you select a patch, all its dependencies are automatically selected as well. 
For example, base_tsa is required for most patches targetting the TSA games. These patches should include nmlgc/base_tsa as a dependency, so that it is automatically selected when these patches are selected. 
Dependencies are recursive. For example, lang_fr doesn't need to depend on base_tsa because it already depends on lang_en, which itself depends on base_tsa.

Dependencies can be listed as either repo_name/patch_name or as only patch_name. If only patch_name is provided, the dependency is pulled from the same repo as the patch.

### servers
The server(s) where the repository can be downloaded. If there is several servers, they must have the exact same files. thcrap_configure may try to download files on different servers in order to optimize bandwidth. 
The hierarchy of files on the server should be the same at on disk.

### neighbors
URLs where other repositories can be found.

The list of repositories isn't hardcoded in thcrap (it would be bothersome to update every time a new repository is added), so thcrap needs a way to find them. 
It starts with a list of known repositories, then every repository can advertize new repositories to check out. thcrap_configure will then check these new repositories, which can also advertize new repositories themselves. All these repositories are "the thcrap network".

In practice, the thpatch repo is the core of the thcrap network. thcrap_configure only knows the URL of the thpatch repositories, and if the thpatch server is dead, thcrap ships with a local copy of the thpatch repo.js. And the thpatch repo.js contains every repo of the thcrap network in its neighbors field. Other repos could add new repos in their neighbors field, but I don't think any does.

Note that thcrap_configure takes the root URL as a command-line parameter. If you don't want to use thcrap with the thcrap network, you can. Remove the thpatch repo.js, and run thcrap_configure with the URL of your repo as parameter. Thcrap will then use your repo network instead of the thcrap one (or, only your repo if you don't have any neighbors).
Of course, if you don't want to pull the whole thcrap network, you shouldn't pull a repo of the thcrap network in your neighbors. If you need base_tsa, you will need to fork it and remove (or edit) the neighbors field.
