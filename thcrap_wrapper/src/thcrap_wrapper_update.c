//#define UNICODE
//#define _UNICODE
#include <windows.h>
#include <tchar.h>

/*
https://docs.microsoft.com/en-us/windows/desktop/menurc/using-resources

// Open the file to which you want to add the dialog box resource.
hUpdateRes = BeginUpdateResource(TEXT("foot.exe"), FALSE);
if (hUpdateRes == NULL)
{
    ErrorHandler(TEXT("Could not open file for writing."));
    return;
}

// Add the dialog box resource to the update list.
result = UpdateResource(hUpdateRes,    // update resource handle
    RT_DIALOG,                         // change dialog box resource
    MAKEINTRESOURCE(IDD_FOOT_ABOUTBOX),         // dialog box id
    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),  // neutral language
    lpResLock,                         // ptr to resource info
    SizeofResource(hExe, hRes));       // size of resource info

if (result == FALSE)
{
    ErrorHandler(TEXT("Could not add resource."));
    return;
}

// Write changes to FOOT.EXE and then close it.
if (!EndUpdateResource(hUpdateRes, FALSE))
{
    ErrorHandler(TEXT("Could not write changes to file."));
    return;
}
*/

WCHAR *copyString(LPWSTR dst, LPCWSTR src)
{
    if (src == NULL) {
        dst[0] = 0;
        return dst + 1;
    }

    WCHAR i;
    for (i = 0; src[i]; i++) {
        dst[i + 1] = src[i];
    }
    dst[0] = i;
    return dst + i + 1;
}

LPVOID loadIcon(HMODULE hModule, DWORD *iconSize)
{
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(0), RT_ICON);
    if (hRes == NULL) return NULL;
    HGLOBAL hResLoad = LoadResource(hModule, hRes);
    if (hResLoad == NULL) return NULL;
    LPVOID icon = LockResource(hResLoad);
    if (icon == NULL) return NULL;
    *iconSize = SizeofResource(hModule, hRes);
    return icon;
}

BOOL updateIcon(HANDLE hUpdateRes, LPCTSTR source)
{
    HMODULE hModule = LoadLibraryEx(source, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!hModule) {
        return FALSE;
    }

    DWORD size;
    LPVOID icon = loadIcon(hModule, &size);
    if (icon == NULL) {
        FreeLibrary(hModule);
        return FALSE;
    }

    if (UpdateResource(hUpdateRes, RT_ICON, MAKEINTRESOURCE(0), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), icon, size) == FALSE) {
        FreeLibrary(hModule);
        return FALSE;
    }

    FreeLibrary(hModule);
    return TRUE;
}

/*
** Update some resources in the exe file [path].
** [path] must be a program compiled from thcrap_wrapper.c
** [lpApplicationName], [lpCommandLine] and [lpCurrentDirectory] will be used
** for the CreateProcess call in thcrap_wrapper. They can be NULL.
** [iconSource] is the path of another exe file. If it is not NULL, its first icon
** will be copied to the 1st resource icon of [path].
*/
BOOL update(LPCTSTR path, LPCWSTR lpApplicationName, LPCWSTR lpCommandLine, LPCWSTR lpCurrentDirectory, LPCTSTR iconSource)
{
    HANDLE hUpdateRes = BeginUpdateResource(path, FALSE);
    if (hUpdateRes == NULL) {
        MessageBox(NULL, TEXT("BeginUpdateResource failed"), NULL, MB_OK);
        return FALSE;
    }

    WCHAR *buffer = malloc((wcslen(lpApplicationName) + wcslen(lpCommandLine) + wcslen(lpCurrentDirectory) + 16) * sizeof(WCHAR));
    WCHAR *pos = buffer;

    pos = copyString(pos, lpApplicationName);
    pos = copyString(pos, lpCommandLine);
    pos = copyString(pos, lpCurrentDirectory);
    for (int i = 3; i < 16; i++) {
        pos[0] = 0;
        pos++;
    }

    if (UpdateResource(hUpdateRes, RT_STRING, MAKEINTRESOURCE(0), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), buffer, (pos - buffer) * sizeof(WCHAR)) == FALSE) {
        MessageBox(NULL, TEXT("UpdateResource failed"), NULL, MB_OK);
        EndUpdateResource(hUpdateRes, TRUE /* Discard changes */);
        return FALSE;
    }

    // Optionnaly copy the icon from another exe
    if (iconSource && !updateIcon(hUpdateRes, iconSource)) {
        MessageBox(NULL, TEXT("Could not update icon"), NULL, MB_OK);
        EndUpdateResource(hUpdateRes, TRUE /* Discard changes */);
        return FALSE;
    }

    EndUpdateResource(hUpdateRes, FALSE /* Don't discard changes */);
    return TRUE;
}

/*
** List of resources:
** lpApplicationName    (param 1: NULL)
** lpCommandLine        (param 2: cmd)
** lpCurrentDirectory   (param 3: cd)
*/
int wmain(int argc, wchar_t *argv[])
{
    if (argc != 5 && argc != 6) {
        wprintf(L"Usage: %s launcher.exe lpApplicationName lpCommandLine lpCurrentDirectory [iconSource.exe]\n", argv[0]);
        return 1;
    }
    update(argv[1] ?: NULL,
        argv[2] ?: NULL,
        argv[3] ?: NULL,
        argv[4] ?: NULL,
        argv[5] ?: NULL);
    return 0;
}
