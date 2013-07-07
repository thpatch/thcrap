Touhou Community Reliant Automatic Patcher
------------------------------------------

### Description ###

Basically, this is an almost-generic, easily expandable and customizable framework to patch Windows applications in memory, specifically tailored towards the translation of Japanese games.

It is mainly developed to facilitate self-updating, multilingual translation of the [Touhou Project] (http://en.wikipedia.org/wiki/Touhou_Project) games on [Touhou Patch Center] (http://thpatch.net/), but can theoretically be used for just about any other patch for these games, without going through that site.

#### Main features of the base engine #####

* Easy **DLL injection** of the main engine and plug-ins into the target process.

* Uses **JSON for all patch configuration data**, making the patches themselves open-source by design. By recursively merging JSON objects, this gives us...

* **Patch stacking** - apply any number of patches at the same time, sorted by a priority list. Supports wildcard-based blacklisting of files in certain patches through the run configuration.

* Automatically adds **transparent Unicode filename support** via Win32 API wrappers to the target program's process, without the need for programs like [AppLocale] (http://en.wikipedia.org/wiki/AppLocale).

* Patches can support **multiple builds and versions** of a single program, identified by a combination of SHA-256 hashes and .EXE file sizes.

* **Binary hacks** for arbitrary in-memory modifications of the original program (mostly used for custom assembly).

* **Breakpoints** to call a custom DLL function at any instruction of the original code. This function can read and modify the current CPU register state.

 * **File breakpoints** to replace data files in memory with replacements from patches.

* Wildcard-based **file format patching hooks** called on file replacements - can apply patches to data files according to a (stackable!) JSON description.

* ... and all that without any significant impact on performance. ☺

### Modules included ###

* `win32_utf8`: A UTF-8 wrapper library around the Win32 API calls we require. This is a stand-alone project and can (and should) be freely used in other applications, too.
* `thcrap`: The main patch engine.
* `thcrap_loader`: A command-line loader to call the injection functions of `thcrap` on a newly created process.
* `thcrap_configure`: A rather cheap command-line patch configuration utility. Will eventually be replaced with a GUI tool.
* `thcrap_tsa`: A thcrap plug-in containing patch hooks for games using the STG engine by Team Shanghai Alice.
* `thcrap_update`: A thcrap plug-in containing updating functionality for patches.

### Building ###

As of now, all subprojects only include a Visual C++ 2010 project file for building. SP1 is recommended, if only for correct version identification in Explorer. Build configurations for different systems are always welcome.

The only dependency is [Jansson] (http://www.digip.org/jansson/), which is required for every module apart from `win32_utf8`. Compile it from the [latest source] (https://github.com/akheron/jansson), then add its include and library directories to every project.

### License ###

The Touhou Community Reliant Patcher and all accompanying modules are licensed under the Do What The Fuck You Want To Public License ([WTFPL] (http://www.wtfpl.net/)), unless stated otherwise. This should eliminate any confusion about licensing terms, as well as any possible inquiries on that matter.

This license is very appropriate given the subject matter. We realize that the Touhou fandom itself thrives by everyone just *doing what the fuck they want to*, ripping off everyone else in the process and not respecting the ego of individuals. Having benefited from that mindset ourselves, we indeed embrace this state of affairs.

Thus, in such a non-serious, fragmented global environment that would not even exist without piracy of the source material, the very idea of a license seems both hypocritical and pointless. It can only possibly have a negative impact on the people who *choose* to respect it in the first place.

Even if we *did* choose a more restrictive license, our chances of actually *enforcing* it are very slim, especially once language barriers come into play (hello, Chinese hackers). 

But really, we don't want our ego to stand in the way of you doing awesome stuff with this. Repackage a custom build with nice artwork to promote your own community? Awesome! Rebrand the project to focus on visual novel translation via private servers? Awesome! Change large sections of the code on your own, creating a superior project with better PR and greater reach in the process? Awesome, we might even jump ship and support yours instead!

That said, we *do* appreciate attribution. ☺

### Contributing ###

This project is actively developed, and contributions are always welcome.

We track the development status of future features on the GitHub Issues page of this repository. Feel free to chip in, discuss implementation details and contribute code - we would *really* appreciate being able to share the workload between a number of developers.

If, for whatever reason, you don't want to go through GitHub, you can also contact Nmlgc by mail under nmlgc@thpatch.net.

### Plug-in development ###

At this stage, we recommend that you regularly send us pull requests for whatever custom plug-ins you develop around the base engine. The API of the base engine is far from finalized and may be subject to change. In case we *do* end up changing something, these changes can then directly be reflected in your project.
