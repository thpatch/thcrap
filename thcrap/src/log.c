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

void log_wprint(const wchar_t *str)
{
	size_t str_len;
	if(!str) {
		return;
	}
	str_len = wcslen(str) + 1;
	{
		VLA(char, str_utf8, str_len * UTF8_MUL);
		StringToUTF8(str_utf8, str, str_len);
		log_print(str_utf8);
		VLA_FREE(str_utf8);
	}
}

void log_nprint(const char *str, size_t n)
{
	fwrite(str, n, 1, stdout);
	if(log_file) {
		fwrite(str, n, 1, log_file);
	}
}

void log_printf(const char *str, ...)
{
	va_list va;
	size_t str_full_len;

	if(!str) {
		return;
	}
	va_start(va, str);
	str_full_len = _vscprintf(str, va) + 1;
	{
		VLA(char, str_full, str_full_len);
		// vs*n*printf is not available in msvcrt.dll. Doesn't matter anyway.
		vsprintf(str_full, str, va);
		va_end(va);
		log_print(str_full);
		VLA_FREE(str_full);
	}
}

void log_wprintf(const wchar_t *str, ...)
{
	va_list va;
	size_t str_full_len;

	if(!str) {
		return;
	}
	va_start(va, str);
	str_full_len = _vscwprintf(str, va) + 1;
	{
		VLA(wchar_t, str_full, str_full_len);
		vsnwprintf(str_full, str_full_len, str, va);
		va_end(va);
		log_wprint(str_full);
		VLA_FREE(str_full);
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

int log_wmbox(const wchar_t *caption, const UINT type, const wchar_t *text)
{
	int ret;
	const char *project_name = PROJECT_NAME();
	WCHAR_T_DEC(project_name);

	if(!text) {
		return 0;
	}
	if(!caption) {
		caption = StringToUTF16_VLA(project_name_w, project_name, project_name_len);
	}
	log_wprint(text); // why not
	ret = MessageBoxW(NULL, text, caption, type);
	VLA_FREE(project_name_w);
	return ret;
}

int log_mboxf(const char *caption, const UINT type, const char *text, ...)
{
	va_list va;
	size_t text_full_len;

	if(!text) {
		return 0;
	}
	va_start(va, text);
	text_full_len = _vscprintf(text, va) + 1;
	{
		int ret;
		VLA(char, text_full, text_full_len);
		// vs*n*printf is not available in msvcrt.dll. Doesn't matter anyway.
		vsprintf(text_full, text, va);
		va_end(va);
		ret = log_mbox(caption, type, text_full);
		VLA_FREE(text_full);
		return ret;
	}
}

int log_wmboxf(const wchar_t *caption, const UINT type, const wchar_t *text, ...)
{
	va_list va;
	size_t text_full_len;

	if(!text) {
		return 0;
	}
	va_start(va, text);
	text_full_len = _vscwprintf(text, va) + 1;
	{
		int ret;
		VLA(wchar_t, text_full, text_full_len);
		vsnwprintf(text_full, text_full_len, text, va);
		va_end(va);
		ret = log_wmbox(caption, type, text_full);
		VLA_FREE(text_full);
		return ret;
	}
}

static void OpenConsole()
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
		VLA(char, line, line_len * UTF8_MUL);

		for(i = 0; i < line_len - 1; i++) {
			// HARRY UP, anyone?
			line[i*3+0] = 0xe2;
			line[i*3+1] = 0x80;
			line[i*3+2] = 0x95;
		}
		line[i*3] = 0;

		fprintf(log_file, "%s\n", line);
		fprintf(log_file, "%s fichier log\n", PROJECT_NAME());
		fprintf(log_file, "version: %s\n", PROJECT_VERSION_STRING());
		fprintf(log_file, "temps de construction: "  __TIMESTAMP__ "\n");
#if defined(BUILDER_NAME_W)
		{
			size_t builder_len = wcslen(BUILDER_NAME_W) + 1;
			VLA(char, builder, builder_len * UTF8_MUL);
			StringToUTF8(builder, BUILDER_NAME_W, builder_len);
			fprintf(log_file, "construit par: %s\n", builder);
			VLA_FREE(builder);
		}
#elif defined(BUILDER_NAME)
		fprintf(log_file, "construit par: %s\n", BUILDER_NAME);
#endif
		fprintf(log_file, "%s\n\n", line);
		fflush(log_file);
		VLA_FREE(line);
	}
#endif
}

void log_exit()
{
	if(console_open) {
		FreeConsole();
	}
	if(log_file) {
		fclose(log_file);
	}
}
