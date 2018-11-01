Touhou Community Reliant Automatic Patcher
------------------------------------------

[![Join the chat at http://discord.thpatch.net](https://discordapp.com/api/guilds/213769640852193282/widget.png)](http://discord.thpatch.net/)

### Description ###

Basically, this is an almost-generic, easily expandable and customizable framework to patch Windows applications in memory, specifically tailored towards the translation of Japanese games.

It is mainly developed to facilitate self-updating, multilingual translation of the [Touhou Project](http://en.wikipedia.org/wiki/Touhou_Project) games on [Touhou Patch Center](http://thpatch.net/), but can theoretically be used for just about any other patch for these games, without going through that site.

#### Main features of the base engine #####

* Easy **DLL injection** of the main engine and plug-ins into the target process.

* **Full propagation to child processes**. This allows the usage of other third-party patches which also use DLL injection, together with thcrap. (Yes, this was developed mainly for vpatch.)

* Uses **JSON for all patch configuration data**, making the patches themselves open-source by design. By recursively merging JSON objects, this gives us...

* **Patch stacking** - apply any number of patches at the same time, sorted by a priority list. Supports wildcard-based blacklisting of files in certain patches through the run configuration.

* Automatically adds **transparent Unicode filename support** via Win32 API wrappers to target processes using the Win32 ANSI functions, without the need for programs like [AppLocale](http://en.wikipedia.org/wiki/AppLocale).

* Patches can support **multiple builds and versions** of a single program, identified by a combination of SHA-256 hashes and .EXE file sizes.

* **Binary hacks** for arbitrary in-memory modifications of the original program (mostly used for custom assembly).

* **Breakpoints** to call custom DLL functions at any instruction of the original code. These functions can read and modify the current CPU register state.

* **Multiple sets** of sequentially applied binary hacks and breakpoints, for working around EXE packers and DRM schemes.

 * **File breakpoints** to replace data files in memory with replacements from patches.

* Wildcard-based **file format patching hooks** called on file replacements - can apply patches to data files according to a (stackable!) JSON description.

* Optional **Steam integration** for games that are available through Steam, but don't come with Steam integration themselves. Can be disabled by deleting `steam_api.dll`.

* ... and all that without any significant impact on performance. ☺

### Modules included ###

* `win32_utf8`: A UTF-8 wrapper library around the Win32 API calls we require. This is a stand-alone project and can (and should) be freely used in other applications, too.
* `thcrap`: The main patch engine.
* `thcrap_loader`: A command-line loader to call the injection functions of `thcrap` on a newly created process.
* `thcrap_configure`: A GUI wizard for discovering patches, configuring patch stacks, and locating supported games.
* `thcrap_tsa`: A thcrap plug-in containing patch hooks for games using the STG engine by Team Shanghai Alice.
* `thcrap_tasofro`: A thcrap plug-in containing patch hooks for various games by [Twilight Frontier](http://tasofro.net/).
* `thcrap_update`: Contains updating functionality for patches, digitally signed automatic updates of thcrap itself, as well as an updater GUI. `thcrap_update.dll` can be safely deleted to disable all online functionality.
* `thcrap_bgmmod`: A helper library to handle the non-game-specific parts of BGM modding for originally uncompressed PCM music, like codec support and loop point handling. Currently statically linked into `thcrap_tsa` as this module is currently the only one with support for BGM modding, but already split into a separate library to be ready for covering more engines in the future.

### Building ###

A ready-made Visual Studio build configuration, covering all modules and their dependencies, is provided as part of this repository. To set up the build:

* Install [Visual Studio Community 2013](https://visualstudio.microsoft.com/vs/older-downloads/).
* Make sure that you've pulled all Git submodules together with this repo:

		git clone --recursive https://github.com/thpatch/thcrap.git

* (Optional) If your thcrap build should be able to automatically update itself, you need to create a code signing certificate. To do this, run the following commands on the Visual Studio command prompt (`vcvarsall.bat`) in the root directory of this repo (the one with `thcrap.sln`):

		makecert -n "CN=Your Name,E=yourmail@provider.net" -$ individual -a sha256 -len 4096 -r -cy authority -sky signature -pe -sv cert.pvk cert.cer
		pvk2pfx -pvk cert.pvk -spc cert.cer -pfx cert.pfx

	`cert.pfx` is used to sign the binaries as part of the build, so don't change the file name.

Then, open `thcrap.sln`, choose Debug or Release from the drop-down menu in the toolbar (or the Configuration Manager) and run *Build → Build Solution* from the main menu.

You can also build from the command line by running the Visual Studio tool environment batch file (`vcvarsall.bat`), then run

		msbuild /m /p:Configuration=Debug

or

		msbuild /m /p:Configuration=Release

in the thcrap directory. The binaries will end up in the `bin/` subdirectory.

#### Signing a release archive for automatic updates ####
First, convert `cert.pvk` to a .pem file using OpenSSL, then use this file together with `scripts/release_sign.py`:

	openssl rsa -inform pvk -in cert.pvk -outform pem -out cert.pem
	python release_sign.py -k cert.pem thcrap.zip

#### Using different compilers ####

Visual Studio Community 2013 is recommended for building, and the build configuration references the Visual Studio 2013 platform toolset with Windows XP targeting support by default. However, the project should generally build under every version since Visual C++ 2010 Express after changing the `<PlatformToolset>` value in `Base.props`. For a list of all platform toolsets available on your system, open the `Properties` dialog for any included project and refer to the drop-down menu at *Configuration Properties → General → Platform Toolset*.

Compilation with MinGW is currently not supported. This is not likely to change in the foreseeable future as we don't see much value in it.

#### Dependencies ####

All required third-party libraries for the C/C++ code are included as Git submodules. These are:

* [Jansson](http://www.digip.org/jansson/), required for every module apart from `win32_utf8`.

* [libpng](http://www.libpng.org/pub/png/libpng.html) **(>= 1.6.0)**, required by `thcrap_tsa` for image patching.

* [zlib](http://zlib.net/), required by `thcrap_update` for CRC32 verification. It's required by `libpng` anyway, though.

* `thcrap_bgmmod` currently supports the following codecs via third-party libraries:

  * **FLAC** via [dr_flac](https://mackron.github.io/dr_flac.html)
  * **Ogg Vorbis** via [libogg](https://xiph.org/ogg/), [libvorbis](https://xiph.org/vorbis/) and libvorbisfile

The scripts in the `scripts` directory are written in [Python 3](http://python.org/). Some of them require further third-party libraries not included in this repository:

* [PyCrypto](https://www.dlitz.net/software/pycrypto/) is required by `release_sign.py`.
* [pathspec](https://pypi.python.org/pypi/pathspec) is required by `repo_update.py`. Can be easily installed via `pip`.

### License ###

The Touhou Community Reliant Patcher and all accompanying modules are released to the Public Domain, unless stated otherwise. This means you can do whatever you want with this code without so much as crediting us.

That said, we *do* appreciate attribution. ☺
