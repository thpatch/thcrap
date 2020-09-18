BUILD64=1

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
CFLAGS += -municode
CFLAGS += -Wall -Wextra
# We want to ignore these warnings.
# -Wno-unknown-pragmas: our main build platform is still MSVC, we want
# to keep the pragmas on MSVC and deal with it on gcc.
# -Wunused-local-typedefs: in win32_utf8, we want to keep all the typedef
# declarations, even if we don't use them, in order to match how they
# were originally declared in Windows.
# -Wno-cast-function-type: it tends to warn on FARPROC casts, which are
# needed in a lot of cases
CFLAGS += -Wno-unknown-pragmas -Wno-unused-local-typedefs -Wno-cast-function-type
CFLAGS += -I. -Ilibs -Ilibs/external_deps -Ithcrap/src -Ilibs/win32_utf8

# TODO: fix and remove
CFLAGS += -Wno-unused-but-set-variable -Wno-sign-compare

CXXFLAGS = $(CFLAGS) -std=c++17

%.o : %.asm
	$(AS) -o $@ $<

# Everything else is pulled from dependencies
# all: bin/bin/thcrap_configure.exe bin/bin/thcrap_loader.exe bin/bin/thcrap_test.exe
all: bin/bin/thcrap_test.exe



THCRAP_DLL_SRCS = \
	thcrap/src/dialog.cpp \
	thcrap/src/exception.cpp \
	thcrap/src/fonts_charset.cpp \
	thcrap/src/jsondata.cpp \
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

THCRAP_DLL_OBJS = $(THCRAP_DLL_SRCS:.cpp=.o)
$(THCRAP_DLL_OBJS): CXXFLAGS += -DTHCRAP_EXPORTS
# It casts a "DWORD token value" to a pointer, which seems weird
# and have no chances to work on 64-bits. We will need to look
# at it for 64-bits support... which isn't a priority right now.
# And minid3d.h is included by tlnote.cpp.
thcrap/src/minid3d.o: CXXFLAGS += -Wno-int-to-pointer-cast
thcrap/src/tlnote.o:  CXXFLAGS += -Wno-int-to-pointer-cast

THCRAP_DLL_LDFLAGS = -shared -Lbin/bin -lwin32_utf8 -ljansson -lzlib-ng -lgdi32 -lshlwapi -luuid -lole32 -lpsapi -lwinmm -Wl,--enable-stdcall-fixup

ifneq ($(BUILD64),1)
THCRAP_DLL_OBJS += thcrap/src/bp_entry.o
else
THCRAP_DLL_LDFLAGS += -Wl,--defsym=bp_entry=0 -Wl,--defsym=bp_entry_localptr=0 -Wl,--defsym=bp_entry_end=0
endif

bin/bin/thcrap.dll: bin/bin/win32_utf8.dll bin/bin/jansson.dll bin/bin/zlib-ng.dll $(THCRAP_DLL_OBJS) thcrap/thcrap.def
	$(CXX) $(THCRAP_DLL_OBJS) thcrap/thcrap.def -o bin/bin/thcrap.dll $(THCRAP_DLL_LDFLAGS)



THCRAP_UPDATE_SRCS = \
	thcrap_update/src/downloader.cpp \
	thcrap_update/src/download_url.cpp \
	thcrap_update/src/file.cpp \
	thcrap_update/src/http_status.cpp \
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

THCRAP_UPDATE_LDFLAGS = -shared -lgdi32 -lcrypt32 -lwininet -lshlwapi -lcomctl32 -Lbin/bin -lwin32_utf8 -lthcrap -ljansson

bin/bin/thcrap_update.dll: bin/bin/thcrap.dll $(THCRAP_UPDATE_OBJS) thcrap_update/thcrap_update.def
	$(CXX) $(THCRAP_UPDATE_OBJS) thcrap_update/thcrap_update.def -o bin/bin/thcrap_update.dll $(THCRAP_UPDATE_LDFLAGS)



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
	$(CXX) $(THCRAP_TEST_OBJS) -o bin/bin/thcrap_test.exe -Lbin/bin -ljansson -lthcrap -lthcrap_update -Wl,-subsystem,console



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
	$(CC) $(JANSSON_DLL_OBJS) -o bin/bin/jansson.dll -shared



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
	$(CC) $(ZLIB_NG_DLL_OBJS) -o bin/bin/zlib-ng.dll -shared



WIN32_UTF8_DLL_SRCS = libs/win32_utf8/win32_utf8_build_dynamic.c

WIN32_UTF8_DLL_OBJS = $(WIN32_UTF8_DLL_SRCS:.c=.o)

bin/bin/win32_utf8.dll: $(WIN32_UTF8_DLL_OBJS)
	$(CC) $(WIN32_UTF8_DLL_OBJS) -o bin/bin/win32_utf8.dll -shared -ldelayimp -lcomdlg32 -ldsound -lgdi32 -lole32 -lpsapi -lshell32 -lshlwapi -luser32 -lversion -lwininet

clean:
	rm -f $(THCRAP_DLL_OBJS) $(THCRAP_TEST_OBJS) $(JANSSON_DLL_OBJS) $(ZLIB_NG_DLL_OBJS) $(WIN32_UTF8_DLL_OBJS)

.PHONY: clean
