# Contributing
This is a set of guidelines to contribute to this repo. If you have any question about them, or about something that you think is missing here, you can ask us on [our Discord](https://discord.thpatch.net/) (you can also join for any other reason, it's open to everyone).

## Code style
We tend to stay close to the default indent style for Visual Studio in C++ mode. 
We use tabs for indentation. For everything else, try to follow what the file you're editing does (or for a new file, look at the files around it). 
One significant change we have over most code styles is the way we use if/else blocks. Always put the opening bracket on the same line, and always have the closing bracket on an empty line. Even with else. 
For example:
```c
if (something) {
    // Do something
}
else if (something_else) {
    // Do something else
}
else {
    // Do a 3rd thing
}
```
I think this style provides a good readability, and makes it trivial to comment a branch. 
If you think code readability can be improved by breaking a rule in some special cases, do it. For example, someting like this would be acceptable:
```c
if      (strcmp(str, "abc") == 0) do_abc();
else if (strcmp(str, "def") == 0) do_def();
```

If you fix the formatting of a file while making a change, please have the formatting changes in one commit and the actual features / fixes in another commit. This makes the interesting commit smaller and easier to read.

## Commit style
For commit messages, use the standard git format: a title, an empty line, and a description. Try to keep the title under 50 characters, and every line in the description under 80 characters. 
Prefix the title with the component affected. 
The description is optionnal if the title contains enough informations to know what happened in the commit.

For example:
```
configure: sort games in games.js

The games list is sorted 2 times, a 1st time before the user
chooses between different versions of a game, and then a 2nd time
after merging with the previous games list in order to keep the
games.js on disk sorted.
```

## Compiler
The release is built with Visual Studio 2017. Our usual setup is to install Visual Studio 2019, and install the v141_xp toolset (the Visual Studio 2017 compiler with Windows XP compatibility) from the Visual Studio 2019 installer. 
We stay on the v141_xp toolset because it is the last one to support Windows XP.

Any C++ feature properly supported by this compiler can be used. This includes C++17 features, but not C++20 features.

## gcc
There is a Makefile for gcc, but no official bulid using it. At least for now, it's mostly for me, when I want to work on a few things but I don't have a Windows PC at hand. You don't need to test your code with it.

## Build files
We like having clean build files. All build files are hand-written, with many build parameters being shared between most projects. 
If you want to add a build parameter that should be used by every project (for example, the version of the compiler toolset), put that in Base.props. Debug and release specific flags go to Debug.props and Release.props. For thcrap-specific flags (like the include path for thcrap.h), use thcrap.props.

## Dependencies
The project **must** build with these 3 steps:
- `git clone --recursive https://github.com/thpatch/thcrap`
- Open Visual Studio
- Rebuild all

If you add new dependencies, you must ensure this workflow still works. For header-only dependencies, you can add them as a Git submodule so that they are cloned automatically when running `git clone --recursive`. For compiled code, you must either use a prebuilt (that can be provided in the thcrap_external_deps repo), or have build rules that build the module when using "Rebuild all".

## API
DLLs must provide a C-compatible API. In the future, some DLLs may be compiled with a different compiler (see issue #48), and a C-compatible API is required for this to work. 
They can also provide some C++ APIs, as long as a compatible C API is available. Don't abuse this. 
C++ functions can also be exported for testing.

The jansson library is part of the API, but should not be abused. If you need to pass a json file between 2 components, you can pass a `json_t*` object. If you need to pass something that *isn't* a JSON object between 2 components (like a key-value mapping), *don't* use a `json_t*` object, find a way to represent that with C structures.

For interactions between 2 components inside the same DLL, you can use a C++ API if you want to.

## JSON
We mostly use JSON for disk and network I/O. Of course, we don't use it when it doesn't make sense - we write our log files in txt format, and we read our image files in PNG format. But for everything that can be represented as JSON, we use JSON files with the .js extension. 
We also have JSON files with the .jdiff extension. These files are used when looking for a file in the patch stack. Every jdiff file will be applied over the game file.

## Unicode
A big part of this project is about fixing programs that don't handle Unicode well, so it's important that we handle Unicode well ourselves.

In most places, we UTF-8 stored in `char*` variables. The jansson library uses UTF-8 jansson files, and the `json_string` strings are always in UTF-8. 
When we interact with Windows, unless we really need to work with UTF-16 directly for some reasin, we use win32_utf8 to handle the conversion between UTF-8 and UTF-16 silently. If you need to use a Windows function that isn't available in win32_utf8, you can call the UTF-16 version, but we would prefer if you could add it to win32_utf8 (we are the mainteners of win32_utf8, so you can open a pull request in both projects and we will take care of them together.

## Backward compatibility
Backward compatibility is important in a few ways:
- Older versions of the self updater must be able to update to the last version.
- Older versions of thcrap_configure should continue to work as much as possible.
- Newer versions of thcrap should stay compatible with older patches formats.

There are 2 rationales behind this:
- Older versions exist.
- We don't own all our patches.

thcrap_configure doesn't auto-update itself, and older versions of thcrap_loader had some trouble updating themselves, often requiring a few restarts. We also made some mistakes in the past, and may make more in the future. And some games download sites bundle a thcrap version with their games, which of course doesn't get updated when we release a new version and has to auto-update itself. 
A few statistics about thcrap versions, taken today on an unspecified time frame, and not filtered by individual IPs:
```
thcrap_update.js (thcrap_loader looking for a self-update):
     70 "thcrap/2019-12-29
    131 "thcrap/2020-06-06
    518 "thcrap/2020-12-08
lang_en/files.js (thcrap_configure or thcrap_loader pulling for the files list):
     32 "thcrap/2019-12-29
     53 "thcrap/2019-07-18
    208 "thcrap/2020-06-06
   1652 "thcrap/2020-12-08
```
The 2020-12-08 version is the last one, and the 2020-06-06 version has a bug where the self updater doesn't work. We can see many people stayed on 2020-06-06. I even see a few versions from 2017 in the logs.

Because older versions exist on the net, every version **must** be able to auto-update to the last version. The self updater must have full backward compatibility, and provide an automatic update path when breaking changes to the self updater are introduced.

Backward compatibility of new patches / new patch features / new games with older versions of thcrap isn't important. If they are using the latest patch, they are expected to use the last version of thcrap.

Backward compatibility of older patches (and most importantly the language patches and their dependencies, which are the most likely to be used by people who don't update), is rather important. If we *need* to make a breaking change, we can make one, but we would rather keep things working for everyone. Same for repo.js.

Backward compatibility with older repo.js, patch.js and files.js files is even more important. We don't maintain every patch in the thcrap network. We would need to coordinate with every other patch maker who uses thcrap, and make them update their patches. I'm not even sure we know how to contact everyone who has a patch on the thcrap network. So, a breaking change is possible if needed, but should be avoided.

## Windows XP
We still offer limited support for Windows XP. When doing a small new feature, try to at least keep thcrap working on Windows XP. 
For example, I recently used a Vista-only function to sort keys in a JSON config file, in order to make it more readable. An unsorted JSON file still works, so disabling the feature on XP is an acceptable fix (in this case, there was a XP-compatible function that did the job just as well so it didn't matter). But trying to use the Vista function every time and having thcrap crash on XP is not acceptable.

For major changes, like a new UI, we might consider dropping Windows XP support. You should come and discuss about it with us on [our Discord](https://discord.thpatch.net/).

## Testing
For new features, we would appreciate some unit tests coming with them. But due to our really small test coverage, we do not require unit tests - we won't ask for more effort from external contributors than what we do ourselves. 
If you change one of the parts that does have unit tests, please update them.
