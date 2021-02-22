#pragma once
#include <windows.h>
#include <string>
void console_init();
void con_printf(const char *str, ...);
void con_clickable(std::wstring &&response);
std::wstring console_read(void);
void cls(SHORT top);
void pause(void);
void console_prepare_prompt(void);
void console_print_percent(int pc);
char console_ask_yn(const char* what);
void con_end(void);
HWND con_hwnd(void);
int console_width(void);
extern bool con_can_close;
