/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Logging functions.
  */

#include "thcrap.h"
#include <io.h>
#include <fcntl.h>

// -------
// Globals
// -------
static FILE *log_file = NULL;
static int console_open = 0;
static const char LOG[] = "thcrap_log.txt";
// -----------------------

#define VLA_VSPRINTF(str, va) \
	size_t str##_full_len = _vscprintf(str, va) + 1; \
	VLA(char, str##_full, str##_full_len); \
	/* vs*n*printf is not available in msvcrt.dll. Doesn't matter anyway. */ \
	vsprintf(str##_full, str, va);

void log_print(const char *str)
{
	if(console_open) {
		fprintf(stdout, str);
	}
	if(log_file) {
		fprintf(log_file, str);
		fflush(log_file);
	}
}

void log_nprint(const char *str, size_t n)
{
	if(console_open) {
		fwrite(str, n, 1, stdout);
	}
	if(log_file) {
		fwrite(str, n, 1, log_file);
	}
}

void log_vprintf(const char *str, va_list va)
{
	if(str) {
		VLA_VSPRINTF(str, va);
		log_print(str_full);
		VLA_FREE(str_full);
	}
}

void log_printf(const char *str, ...)
{
	if(str) {
		va_list va;
		va_start(va, str);
		log_vprintf(str, va);
		va_end(va);
	}
}

/**
  * Message box functions.
  */

int log_mbox(const char *caption, const UINT type, const char *text)
{
	if(!caption) {
		caption = PROJECT_NAME();
	}
	log_print("---------------------------\n");
	log_printf("%s\n", text);
	log_print("---------------------------\n");
	return MessageBox(NULL, text, caption, type);
}

int log_vmboxf(const char *caption, const UINT type, const char *text, va_list va)
{
	int ret = 0;
	if(text) {
		VLA_VSPRINTF(text, va);
		ret = log_mbox(caption, type, text_full);
		VLA_FREE(text_full);
	}
	return ret;
}

int log_mboxf(const char *caption, const UINT type, const char *text, ...)
{
	int ret = 0;
	if(text) {
		va_list va;
		va_start(va, text);
		ret = log_vmboxf(caption, type, text, va);
		va_end(va);
	}
	return ret;
}

static void OpenConsole(void)
{
	int hCrt;
	FILE *hf;

	if(console_open) {
		return;
	}
	// Some magical code copy-pasted from somewhere
	AllocConsole();
	hCrt = _open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	hf = _fdopen(hCrt, "w");
	*stdout = *hf;
	setvbuf(stdout, NULL, _IONBF, 0);

	/// This breaks all normal, unlogged printf() calls to stdout!
	// _setmode(_fileno(stdout), _O_U16TEXT);

	console_open = 1;
}

void log_init(int console)
{
	log_file = fopen(LOG, "wb");
#ifdef _DEBUG
	OpenConsole();
#else
	if(console) {
		OpenConsole();
	}
	if(log_file) {
		size_t i;
		size_t line_len = strlen(PROJECT_NAME()) + strlen(" logfile") + 1;
		VLA(unsigned char, line, line_len * UTF8_MUL);

		for(i = 0; i < line_len - 1; i++) {
			// HARRY UP, anyone?
			line[i*3+0] = 0xe2;
			line[i*3+1] = 0x80;
			line[i*3+2] = 0x95;
		}
		line[i*3] = 0;

		fprintf(log_file, "%s\n", line);
		fprintf(log_file, "%s logfile\n", PROJECT_NAME());
		fprintf(log_file, "Version: %s\n", PROJECT_VERSION_STRING());
		fprintf(log_file, "Build time: "  __DATE__ " " __TIME__ "\n");
#if defined(BUILDER_NAME_W)
		{
			const wchar_t *builder = BUILDER_NAME_W;
			UTF8_DEC(builder);
			UTF8_CONV(builder);
			fprintf(log_file, "Built by: %s\n", builder_utf8);
			UTF8_FREE(builder);
		}
#elif defined(BUILDER_NAME)
		fprintf(log_file, "Built by: %s\n", BUILDER_NAME);
#endif
		fprintf(log_file, "%s\n\n", line);
		fflush(log_file);
		VLA_FREE(line);
	}
#endif
}

void log_exit(void)
{
	if(console_open) {
		FreeConsole();
	}
	if(log_file) {
		fclose(log_file);
	}
}
