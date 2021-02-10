#pragma once
#include <windows.h>
#include <type_traits> // for std::is_same, std::is_convertible
#include <utility> // for std::declval

template<typename T>
class Dialog {
	// T must derive from Dialog<T> and have the following member functions accessible from Dialog<T>:
	// HINSTANCE getInstance();
	// LPCWSTR getTemplate();
	// INT_PTR dialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
	static INT_PTR CALLBACK internalDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		T *obj = NULL;
		if (uMsg == WM_INITDIALOG) {
			SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)lParam);
			obj = (T*)lParam;
			obj->hWnd = hWnd;
		} else {
			obj = fromHandle(hWnd);
		}

		INT_PTR result = FALSE;
		if (obj) {
			result = obj->dialogProc(uMsg, wParam, lParam);

			if (uMsg == WM_NCDESTROY)
				obj->hWnd = NULL;
		}

		return result;
	}
protected:
	HWND hWnd = NULL;
public:
	static T *fromHandle(HWND hWnd) {
		return (T*)GetWindowLongPtrW(hWnd, DWLP_USER);
	}
	bool isAlive() {
		return !!hWnd;
	}
	HWND getHandle() {
		return hWnd;
	}
	void createModal(HWND parent) {
		static_assert(std::is_same<decltype(std::declval<T>().getInstance()), HINSTANCE>::value);
		static_assert(std::is_same<decltype(std::declval<T>().getTemplate()), LPCWSTR>::value);
		static_assert(std::is_same<decltype(std::declval<T>().dialogProc((UINT)0, (WPARAM)0, (LPARAM)0)), INT_PTR>::value);
		static_assert(std::is_convertible<T*, Dialog<T>*>::value);

		if (!isAlive())
			DialogBoxParamW(((T*)this)->getInstance(), ((T*)this)->getTemplate(), parent, internalDialogProc, (LPARAM)(T*)this);
	}
};
