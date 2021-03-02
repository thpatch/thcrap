#pragma once
#include <wchar.h>

#if !defined(THCRAP_I18N_DONTALIAS) && defined(THCRAP_I18N_APPDOMAIN)
#define _(s) (i18n_w(THCRAP_I18N_APPDOMAIN, s))
#define C_(c,s) (i18n_wp(THCRAP_I18N_APPDOMAIN, c, s))
#define N_(s,p,n) (i18n_wn(THCRAP_I18N_APPDOMAIN, s, p, n))
#define NC_(c,s,p,n) (i18n_wpn(THCRAP_I18N_APPDOMAIN, c, s, p, n))

#define _A(s) (i18n_(THCRAP_I18N_APPDOMAIN, s))
#define C_A(c,s) (i18n_p(THCRAP_I18N_APPDOMAIN, c, s))
#define N_A(s,p,n) (i18n_n(THCRAP_I18N_APPDOMAIN, s, p, n))
#define NC_A(c,s,p,n) (i18n_pn(THCRAP_I18N_APPDOMAIN, c, s, p, n))
#endif

/**
* Naming for the functions
* i18n_[w][n][p]
* w - wide
* n - plurals
* p - context
*/

#ifdef __cplusplus
extern "C" {
#endif
#ifdef THCRAP_I18N_DISABLE
#define i18n_translate(d,c,s,p,n) (((n)==1)?(s):(p))
#define i18n_translate_wide(d,c,s,p,n) (((n)==1)?(s):(p))
#define i18n_load_narrow(domain)
#define i18n_load_wide(domain)
#define i18n_load_narrow_only(domain)
#define i18n_load_wide_only(domain)
#define i18n_langid() "en"
#define i18n_lang_selector(domain)
#define i18n_lang_init(domain)
#define i18n_lang_init_quiet()
#else
	/*** translation functions ***/
	const char *i18n_translate(const char *domain, const char *s, const char *def_s, const char *def_p, unsigned long num);
	const wchar_t *i18n_translate_wide(const char *domain, const wchar_t *s, const wchar_t *def_s, const wchar_t *def_p, unsigned long num);

	/*** explicit domain loading functions ***/

	/**
	* Loads and selects a given domain.
	* First call to any of the translation functions will implicitly do this.
	*/
	void i18n_load_narrow(const char *domain);
	void i18n_load_wide(const char *domain);
	/**
	* Loads domain, and preemptively forbids conversion to wide.
	*/
	void i18n_load_narrow_only(const char *domain);
	/**
	* Loads domain, converts it to wide, and then frees the narrow strings.
	* This function is not thread-safe. If a narrow translation func gets called
	* on the same domain while it's being freed, bad stuff happens.
	*/
	void i18n_load_wide_only(const char *domain);

	/*** language functions ***/
	const char* i18n_langid();
	void i18n_lang_selector(const char *domain);
	void i18n_lang_init(const char *domain);
	void i18n_lang_init_quiet();
#endif


#define THCRAP_I18N_CTXSEP "\f"
#define THCRAP_I18N_CTXSEPW L"\f"

#define i18n_(d,s) i18n_translate(d,s,s,s,ULONG_MAX)
#define i18n_p(d,c,s) i18n_translate(d,c THCRAP_I18N_CTXSEP s,s,s,ULONG_MAX)
#define i18n_n(d,s,p,n) i18n_translate(d,s,s,p,n)
#define i18n_pn(d,c,s,p,n) i18n_translate(d,c THCRAP_I18N_CTXSEP s,s,p,n)
#define i18n_w(d,s) i18n_translate_wide(d,s,s,s,ULONG_MAX)
#define i18n_wp(d,c,s) i18n_translate_wide(d,c THCRAP_I18N_CTXSEPW s,s,s,ULONG_MAX)
#define i18n_wn(d,s,p,n) i18n_translate_wide(d,s,s,p,n)
#define i18n_wpn(d,c,s,p,n) i18n_translate_wide(d,c THCRAP_I18N_CTXSEPW s,s,p,n)

#ifdef __cplusplus
}
#endif
