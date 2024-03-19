BUILD64=0

ifneq ($(BUILD64),1)
AS      = i686-w64-mingw32-as
CC      = i686-w64-mingw32-gcc
CXX     = i686-w64-mingw32-g++
WINDRES = i686-w64-mingw32-windres
else
AS      = x86_64-w64-mingw32-as
CC      = x86_64-w64-mingw32-gcc
CXX     = x86_64-w64-mingw32-g++
WINDRES = x86_64-w64-mingw32-windres
endif

CFLAGS =  -DBUILDER_NAME_W=L\"$(USER)\"
CFLAGS += -DPROJECT_VERSION_Y=9999 -DPROJECT_VERSION_M=99 -DPROJECT_VERSION_D=99 '-DDISCOVERY_DEFAULT_REPO="https://srv.thpatch.net/"'
CFLAGS += -municode
CFLAGS += -mfpmath=sse -msse4.1 -msha -mlong-double-80
# -mpreferred-stack-boundary=2 is broken on i686-w64-mingw32-g++ (GCC) 12.2.0
# CFLAGS += -mfpmath=sse -msse4.1 -msha -mlong-double-80 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2
CFLAGS += -Wall -Wextra
# We want to ignore these warnings.
# -Wno-unknown-pragmas: our main build platform is still MSVC, we want
# to keep the pragmas on MSVC and deal with it on gcc.
# -Wunused-local-typedefs: in win32_utf8, we want to keep all the typedef
# declarations, even if we don't use them, in order to match how they
# were originally declared in Windows.
# -Wno-cast-function-type: it tends to warn on FARPROC casts, which are
# needed in a lot of cases.
# -Wno-attributes: triggers a warning when gcc fails to inline a function
# with TH_FORCEINLINE.
# -Wno-unused-function: we have a few that we want to to have in case we
# need to use them in the future.
# -Wno-multistatement-macros: we like having several statements in unlikely
# macros.
CFLAGS += \
	-Wno-unknown-pragmas \
	-Wno-unused-local-typedefs \
	-Wno-cast-function-type \
	-Wno-attributes \
	-Wno-unused-function \
	-Wno-multistatement-macros \

CFLAGS += \
	-I. \
	-Ilibs \
	-Ilibs/external_deps \
	-Ilibs/external_deps/include \
	-Ilibs/json5pp \
	-Ithcrap/src \
	-Ilibs/win32_utf8 \

# TODO: fix and remove
CFLAGS += -Wno-unused-but-set-variable -Wno-sign-compare

CXXFLAGS = $(CFLAGS) -std=c++17
# For rand_s
CXXFLAGS += -D_CRT_RAND_S
# std::unexpected, which is removed in C++17, conflicts with our unexpected() macro.
# This define tells the glibc to remove the deprecated functions.
# ... until std::unexpected comes back as another thing in C++23.
CXXFLAGS += -D_GLIBCXX_USE_DEPRECATED=0

LDFLAGS += -o $@ -Lbin/bin -Llibs/external_deps/bin

%.o : %.asm
	$(AS) -o $@ $<

# Everything else is pulled from dependencies
# TODO: add bin/bin/thcrap_configure.exe bin/bin/thcrap_loader.exe bin/bin/thcrap_tsa.dll bin/bin/thcrap_bgmmod.dll
# TODO: add build rules for bin/bin/thcrap_bgmmod.dll (required by thcrap_tsa)
all: bin/bin/thcrap_test.exe bin/bin/thcrap_tasofro.dll bin/bin/thcrap_update.dll



THCRAP_DLL_SRCS = \
	thcrap/src/dialog.cpp \
	thcrap/src/exception.cpp \
	thcrap/src/expression.cpp \
	thcrap/src/fonts_charset.cpp \
	thcrap/src/jsondata.cpp \
	thcrap/src/ntdll.cpp \
	thcrap/src/mempatch.cpp \
	thcrap/src/binhack.cpp \
	thcrap/src/bp_file.cpp \
	thcrap/src/breakpoint.cpp \
	thcrap/src/init.cpp \
	thcrap/src/log.cpp \
	thcrap/src/global.cpp \
	thcrap/src/jansson_ex.cpp \
	thcrap/src/minid3d.cpp \
	thcrap/src/patchfile.cpp \
	thcrap/src/pe.cpp \
	thcrap/src/plugin.cpp \
	thcrap/src/promote.cpp \
	thcrap/src/repatch.cpp \
	thcrap/src/repo.cpp \
	thcrap/src/runconfig.cpp \
	thcrap/src/search.cpp \
	thcrap/src/sha256.cpp \
	thcrap/src/dict_translate.cpp \
	thcrap/src/inject.cpp \
	thcrap/src/shelllink.cpp \
	thcrap/src/stack.cpp \
	thcrap/src/steam.cpp \
	thcrap/src/strings.cpp \
	thcrap/src/strings_array.cpp \
	thcrap/src/tlnote.cpp \
	thcrap/src/util.cpp \
	thcrap/src/textdisp.cpp \
	thcrap/src/thcrap_update_wrapper.cpp \
	thcrap/src/vfs.cpp \
	thcrap/src/win32_detour.cpp \
	thcrap/src/xpcompat.cpp \
	thcrap/src/zip.cpp \
	thcrap/src/gcc_gs.cpp \

THCRAP_DLL_SRCS_ASM = \
	thcrap/src/inject_func.o \
	thcrap/src/str_to_addr.o \

THCRAP_DLL_OBJS = $(THCRAP_DLL_SRCS:.cpp=.o) $(THCRAP_DLL_SRCS_ASM:.asm=.o)
$(THCRAP_DLL_OBJS): CXXFLAGS += -DTHCRAP_EXPORTS
# It casts a "DWORD token value" to a pointer, which seems weird
# and have no chances to work on 64-bits. We will need to look
# at it for 64-bits support... which isn't a priority right now.
# And minid3d.h is included by tlnote.cpp.
thcrap/src/minid3d.o: CXXFLAGS += -Wno-int-to-pointer-cast
thcrap/src/tlnote.o:  CXXFLAGS += -Wno-int-to-pointer-cast

THCRAP_DLL_LDFLAGS = -shared -Lbin/bin -lwin32_utf8 -ljansson -lzlib-ng -lgdi32 -lshlwapi -luuid -lole32 -lpsapi -lwinmm -Wl,--enable-stdcall-fixup
THCRAP_DLL_LDFLAGS += -lucrt

ifneq ($(BUILD64),1)
THCRAP_DLL_OBJS += thcrap/src/bp_entry.o
else
THCRAP_DLL_LDFLAGS += -Wl,--defsym=bp_entry=0 -Wl,--defsym=bp_entry_localptr=0 -Wl,--defsym=bp_entry_end=0
endif

bin/bin/thcrap.dll: bin/bin/win32_utf8.dll bin/bin/jansson.dll bin/bin/zlib-ng.dll thcrap/thcrap.def $(THCRAP_DLL_OBJS)
	$(CXX) $(THCRAP_DLL_OBJS) thcrap/thcrap.def $(LDFLAGS) $(THCRAP_DLL_LDFLAGS)



THCRAP_UPDATE_SRCS = \
	thcrap_update/src/downloader.cpp \
	thcrap_update/src/download_url.cpp \
	thcrap_update/src/file.cpp \
	thcrap_update/src/http_status.cpp \
	thcrap_update/src/http_curl.cpp \
	thcrap_update/src/http_wininet.cpp \
	thcrap_update/src/loader_update.cpp \
	thcrap_update/src/notify.cpp \
	thcrap_update/src/random.cpp \
	thcrap_update/src/repo_discovery.cpp \
	thcrap_update/src/self.cpp \
	thcrap_update/src/server.cpp \
	thcrap_update/src/thcrap_update.cpp \
	thcrap_update/src/update.cpp \

THCRAP_UPDATE_OBJS = $(THCRAP_UPDATE_SRCS:.cpp=.o)
$(THCRAP_UPDATE_OBJS): CXXFLAGS += -DTHCRAP_UPDATE_EXPORTS -DUSE_HTTP_WININET

THCRAP_UPDATE_LDFLAGS = -shared -lgdi32 -lcrypt32 -lwininet -lshlwapi -lcomctl32 -Lbin/bin -lwin32_utf8 -lthcrap -ljansson -lcurl

bin/bin/thcrap_update.dll: bin/bin/thcrap.dll thcrap_update/thcrap_update.def $(THCRAP_UPDATE_OBJS)
	$(CXX) $(THCRAP_UPDATE_OBJS) thcrap_update/thcrap_update.def $(LDFLAGS) $(THCRAP_UPDATE_LDFLAGS)



THCRAP_TSA_SRCS = \
	thcrap_tsa/src/anm_bounds.cpp \
	thcrap_tsa/src/anm.cpp \
	thcrap_tsa/src/ascii.cpp \
	thcrap_tsa/src/bgm.cpp \
	thcrap_tsa/src/bp_mission.cpp \
	thcrap_tsa/src/devicelost.cpp \
	thcrap_tsa/src/gentext.cpp \
	thcrap_tsa/src/input.cpp \
	thcrap_tsa/src/layout.cpp \
	thcrap_tsa/src/music.cpp \
	thcrap_tsa/src/png_ex.cpp \
	thcrap_tsa/src/spells.cpp \
	thcrap_tsa/src/textimage.cpp \
	thcrap_tsa/src/th06_bp_file.cpp \
	thcrap_tsa/src/th06_bp_music.cpp \
	thcrap_tsa/src/th06_msg.cpp \
	thcrap_tsa/src/th06_pngsplit.cpp \
	thcrap_tsa/src/thcrap_tsa.cpp \
	thcrap_tsa/src/win32_tsa.cpp \

THCRAP_TSA_OBJS = $(THCRAP_TSA_SRCS:.cpp=.o)

THCRAP_TSA_LDFLAGS = -shared

bin/bin/thcrap_tsa.dll: bin/bin/thcrap.dll thcrap_tsa/thcrap_tsa.def $(THCRAP_TSA_OBJS)
	$(CXX) $(THCRAP_TSA_OBJS) thcrap_tsa/thcrap_tsa.def $(LDFLAGS) $(THCRAP_TSA_LDFLAGS)



THCRAP_TASOFRO_SRCS = \
	thcrap_tasofro/src/act-nut.cpp \
	thcrap_tasofro/src/bgm.cpp \
	thcrap_tasofro/src/crypt.cpp \
	thcrap_tasofro/src/csv.cpp \
	thcrap_tasofro/src/cv0.cpp \
	thcrap_tasofro/src/files_list.cpp \
	thcrap_tasofro/src/nhtex.cpp \
	thcrap_tasofro/src/mediawiki.cpp \
	thcrap_tasofro/src/nsml.cpp \
	thcrap_tasofro/src/nsml_images.cpp \
	thcrap_tasofro/src/pl.cpp \
	thcrap_tasofro/src/plaintext.cpp \
	thcrap_tasofro/src/plugin.cpp \
	thcrap_tasofro/src/png.cpp \
	thcrap_tasofro/src/spellcards_generator.cpp \
	thcrap_tasofro/src/tasofro_file.cpp \
	thcrap_tasofro/src/tfcs.cpp \
	thcrap_tasofro/src/th135.cpp \
	thcrap_tasofro/src/th155_bmp_font.cpp \
	thcrap_tasofro/src/th175.cpp \
	thcrap_tasofro/src/th175_json.cpp \
	thcrap_tasofro/src/th175_pl.cpp \
	thcrap_tasofro/src/th175_pl_ed.cpp \
	thcrap_tasofro/src/thcrap_tasofro.cpp \

THCRAP_TASOFRO_OBJS = $(THCRAP_TASOFRO_SRCS:.cpp=.o)
$(THCRAP_TASOFRO_OBJS): CXXFLAGS += -Ilibs/135tk/Act-Nut-lib
# gcc suggests to use parenthesis because the no-parenthesis
# version is ambiguous... and it is indeed ambiguous. So much
# that I'm not sure which version is supposed to be correct
# (I copy-pasted that one from Riatre's th135arc).
# We'll keep it for now, and maybe we'll fix it after writing
# an unit test to ensure the behavior doesn't change.
thcrap_tasofro/src/crypt.o: CXXFLAGS += -Wno-parentheses
# It works, and it's more readable than writing it in hex
thcrap_tasofro/src/act-nut.o: CXXFLAGS += -Wno-multichar
# We do not want to call the other GetGlyphOutline on the chain,
# because we already did the U->W conversion
thcrap_tasofro/src/nsml.o: CXXFLAGS += -Wno-unused-variable

THCRAP_TASOFRO_LDFLAGS = -shared -lgdi32 -lcrypt32 -lwininet -lshlwapi -lcomctl32 \
	-lwin32_utf8 -lthcrap -ljansson -lpng -lzlib-ng -lbmpfont_create -lact_nut_lib \
	-Wl,--enable-stdcall-fixup

THCRAP_TASOFRO_OBJS += thcrap_tasofro/src/bp_bmpfont.o

bin/bin/thcrap_tasofro.dll: bin/bin/thcrap.dll bin/bin/libpng.dll bin/bin/bmpfont_create.dll bin/bin/act_nut_lib.dll thcrap_tasofro/thcrap_tasofro.def $(THCRAP_TASOFRO_OBJS)
	$(CXX) $(THCRAP_TASOFRO_OBJS) thcrap_tasofro/thcrap_tasofro.def $(LDFLAGS) $(THCRAP_TASOFRO_LDFLAGS)



THCRAP_TEST_SRCS = \
	libs/external_deps/googletest/googletest/src/gtest-all.cc \
	libs/external_deps/googletest/googletest/src/gtest_main.cc \
	thcrap_test/src/repo.cpp \
	thcrap_test/src/repo_discovery.cpp \
	thcrap_test/src/runconfig.cpp \
	thcrap_test/src/patchfile.cpp \

THCRAP_TEST_OBJS := $(THCRAP_TEST_SRCS:.cpp=.o)
THCRAP_TEST_OBJS := $(THCRAP_TEST_OBJS:.cc=.o)
$(THCRAP_TEST_OBJS): CXXFLAGS += \
	-Ilibs/external_deps/googletest/googletest \
	-Ilibs/external_deps/googletest/googletest/include \
	-Ithcrap_update/src

bin/bin/thcrap_test.exe: bin/bin/thcrap.dll bin/bin/thcrap_update.dll $(THCRAP_TEST_OBJS)
	$(CXX) $(THCRAP_TEST_OBJS) $(LDFLAGS) -ljansson -lthcrap -lthcrap_update -Wl,-subsystem,console



BMPFONT_DLL_SRCS = \
	libs/135tk/bmpfont/bmpfont_create_main.c \
	libs/135tk/bmpfont/bmpfont_create_core.c \

BMPFONT_DLL_OBJS = $(BMPFONT_DLL_SRCS:.c=.o)
# Using %d in printf instead of %u (32 bits) / %llu (64 bits)
$(BMPFONT_DLL_OBJS): CFLAGS += -Wno-format

bin/bin/bmpfont_create.dll: bin/bin/libpng.dll libs/bmpfont_create.def $(BMPFONT_DLL_OBJS)
	$(CC) $(BMPFONT_DLL_OBJS) libs/bmpfont_create.def $(LDFLAGS) -shared -lpng



ACT_NUT_DLL_SRCS = \
	libs/135tk/Act-Nut-lib/Object.cpp \
    libs/135tk/Act-Nut-lib/Utils.cpp \
    libs/135tk/Act-Nut-lib/exports.cpp \
    libs/135tk/Act-Nut-lib/act/Object.cpp \
    libs/135tk/Act-Nut-lib/act/File.cpp \
    libs/135tk/Act-Nut-lib/act/Entry.cpp \
    libs/135tk/Act-Nut-lib/act/Entries.cpp \
    libs/135tk/Act-Nut-lib/nut/SQObject.cpp \
    libs/135tk/Act-Nut-lib/nut/SQComplexObjects.cpp \
    libs/135tk/Act-Nut-lib/nut/SQInstruction.cpp \
    libs/135tk/Act-Nut-lib/nut/SQFunctionProto.cpp \
    libs/135tk/Act-Nut-lib/nut/Stream.cpp \

ACT_NUT_DLL_OBJS = $(ACT_NUT_DLL_SRCS:.cpp=.o)
$(ACT_NUT_DLL_OBJS): CFLAGS += -Ilibs/135tk/Act-Nut-lib -Wno-multichar

bin/bin/act_nut_lib.dll: $(ACT_NUT_DLL_OBJS)
	$(CXX) $(ACT_NUT_DLL_OBJS) $(LDFLAGS) -shared



JANSSON_DLL_SRCS = \
	libs/external_deps/jansson/src/dump.c \
	libs/external_deps/jansson/src/error.c \
	libs/external_deps/jansson/src/hashtable.c \
	libs/external_deps/jansson/src/hashtable_seed.c \
	libs/external_deps/jansson/src/load.c \
	libs/external_deps/jansson/src/memory.c \
	libs/external_deps/jansson/src/pack_unpack.c \
	libs/external_deps/jansson/src/strbuffer.c \
	libs/external_deps/jansson/src/strconv.c \
	libs/external_deps/jansson/src/utf.c \
	libs/external_deps/jansson/src/value.c \

JANSSON_DLL_OBJS = $(JANSSON_DLL_SRCS:.c=.o)
$(JANSSON_DLL_OBJS): CFLAGS += -DHAVE_CONFIG_H -Wno-unused-parameter

bin/bin/jansson.dll: $(JANSSON_DLL_OBJS)
	$(CC) $(JANSSON_DLL_OBJS) $(LDFLAGS) -shared



LIBPNG_DLL_SRCS = \
	libs/external_deps/libpng/png.c \
    libs/external_deps/libpng/pngerror.c \
    libs/external_deps/libpng/pngget.c \
    libs/external_deps/libpng/pngmem.c \
    libs/external_deps/libpng/pngpread.c \
    libs/external_deps/libpng/pngread.c \
    libs/external_deps/libpng/pngrio.c \
    libs/external_deps/libpng/pngrtran.c \
    libs/external_deps/libpng/pngrutil.c \
    libs/external_deps/libpng/pngset.c \
    libs/external_deps/libpng/pngtrans.c \
    libs/external_deps/libpng/pngwio.c \
    libs/external_deps/libpng/pngwrite.c \
    libs/external_deps/libpng/pngwtran.c \
    libs/external_deps/libpng/pngwutil.c \

LIBPNG_DLL_OBJS = $(LIBPNG_DLL_SRCS:.c=.o)

bin/bin/libpng.dll: $(LIBPNG_DLL_OBJS) bin/bin/zlib-ng.dll
	$(CC) $(LIBPNG_DLL_OBJS) $(LDFLAGS) -shared -lzlib-ng



ZLIB_NG_DLL_SRCS = \
	libs/external_deps/zlib-ng/adler32.c \
	libs/external_deps/zlib-ng/compress.c \
	libs/external_deps/zlib-ng/crc32.c \
	libs/external_deps/zlib-ng/deflate.c \
	libs/external_deps/zlib-ng/deflate_fast.c \
	libs/external_deps/zlib-ng/deflate_medium.c \
	libs/external_deps/zlib-ng/deflate_slow.c \
	libs/external_deps/zlib-ng/functable.c \
	libs/external_deps/zlib-ng/gzclose.c \
	libs/external_deps/zlib-ng/gzlib.c \
	libs/external_deps/zlib-ng/gzread.c \
	libs/external_deps/zlib-ng/gzwrite.c \
	libs/external_deps/zlib-ng/infback.c \
	libs/external_deps/zlib-ng/inffast.c \
	libs/external_deps/zlib-ng/inflate.c \
	libs/external_deps/zlib-ng/inftrees.c \
	libs/external_deps/zlib-ng/match.c \
	libs/external_deps/zlib-ng/trees.c \
	libs/external_deps/zlib-ng/uncompr.c \
	libs/external_deps/zlib-ng/zutil.c \

ZLIB_NG_DLL_OBJS = $(ZLIB_NG_DLL_SRCS:.c=.o)
$(ZLIB_NG_DLL_OBJS): CFLAGS += -DUNALIGNED_OK -DZLIB_COMPAT -Wno-implicit-fallthrough -Wno-sign-compare

bin/bin/zlib-ng.dll: $(ZLIB_NG_DLL_OBJS)
	$(CC) $(ZLIB_NG_DLL_OBJS) $(LDFLAGS) -shared



WIN32_UTF8_DLL_SRCS = libs/win32_utf8/win32_utf8_build_dynamic.c

WIN32_UTF8_DLL_OBJS = $(WIN32_UTF8_DLL_SRCS:.c=.o)

bin/bin/win32_utf8.dll: $(WIN32_UTF8_DLL_OBJS)
	$(CC) $(WIN32_UTF8_DLL_OBJS) $(LDFLAGS) -shared -ldelayimp -lcomdlg32 -ldsound -lgdi32 -lole32 -lpsapi -lshell32 -lshlwapi -luser32 -lversion -lwininet

clean:
	rm -f \
	$(THCRAP_DLL_OBJS)     bin/bin/thcrap.dll \
	$(THCRAP_UPDATE_OBJS)  bin/bin/thcrap_update.dll \
	$(THCRAP_TSA_OBJS)     bin/bin/thcrap_tsa.dll \
	$(THCRAP_TASOFRO_OBJS) bin/bin/thcrap_tasofro.dll \
	$(THCRAP_TEST_OBJS)    bin/bin/thcrap_test.exe \
	$(BMPFONT_DLL_OBJS)    bin/bin/bmpfont_create.dll \
	$(ACT_NUT_DLL_OBJS)    bin/bin/act_nut_lib.dll \
	$(JANSSON_DLL_OBJS)    bin/bin/jansson.dll \
	$(LIBPNG_DLL_OBJS)     bin/bin/libpng.dll \
	$(ZLIB_NG_DLL_OBJS)    bin/bin/zlib-ng.dll \
	$(WIN32_UTF8_DLL_OBJS) bin/bin/win32_utf8.dll \

.PHONY: clean
