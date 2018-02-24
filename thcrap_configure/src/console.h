#pragma once
#include <windows.h>
void console_init();
void con_printf(const char *str, ...);
void con_clickable(const char *response);
void con_clickable(int response);
char* console_read(char *str, int n);
void cls(SHORT top);
void pause(void);
void console_prepare_prompt(void);
void console_print_percent(int pc);
char console_ask_yn(const char* what);
void con_end(void);
