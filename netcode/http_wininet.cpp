#include <Windows.h>
#include <stdexcept>
#include <vector>
#include "http_wininet.h"

double perffreq;

HttpHandle::HttpHandle()
{
    LARGE_INTEGER pf;
    DWORD ignore = 1;
    // DWORD timeout = 500;

    // Format according to RFC 7231, section 5.5.3
    auto self_name = L"thcrap netcode";
    auto self_version = L"0.0.1";
    auto os = L"Windows 10 (hardcoded string)";
    std::wstring agent = std::wstring(self_name) + L"/" + self_version + L" (" + os + L")";

    QueryPerformanceFrequency(&pf);
    perffreq = (double)pf.QuadPart;

    this->internet = InternetOpenW(agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (this->internet == nullptr) {
        // No internet access...?
        throw std::runtime_error("InternetOpen failed");
    }
    /*
    InternetSetOption(ret, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(DWORD));
    InternetSetOption(ret, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(DWORD));
    InternetSetOption(ret, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(DWORD));
    */

    // This is necessary when Internet Explorer is set to "work offline"... which
    // will essentially block all wininet HTTP accesses on handles that do not
    // explicitly ignore this setting.
    InternetSetOption(this->internet, INTERNET_OPTION_IGNORE_OFFLINE, &ignore, sizeof(DWORD));
}

HttpHandle::HttpHandle(HttpHandle&& other)
    : internet(other.internet)
{
    other.internet = nullptr;
}

HttpHandle::~HttpHandle()
{
    if (this->internet) {
        InternetCloseHandle(this->internet);
    }
}

bool HttpHandle::download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback)
{
	DWORD byte_ret = sizeof(DWORD);
	DWORD http_stat = 0;
	HINTERNET hFile = NULL;

	hFile = InternetOpenUrlA(
		this->internet, url.c_str(), NULL, 0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_KEEP_CONNECTION, 0
	);
	if (!hFile) {
		// TODO: We should use FormatMessage() for both coverage and i18n
		// reasons here, but its messages are way too verbose for my taste.
		// So let's wait with that until this code is used in conjunction
		// with a GUI, if at all.
		DWORD inet_ret = GetLastError();
		switch (inet_ret) {
		case ERROR_INTERNET_NAME_NOT_RESOLVED:
			log_printf("Could not resolve hostname\n");
			return false;
		case ERROR_INTERNET_CANNOT_CONNECT:
			log_printf("Connection refused\n");
			return false;
		case ERROR_INTERNET_TIMEOUT:
			log_printf("timed out\n");
			return false;
		case ERROR_INTERNET_UNRECOGNIZED_SCHEME:
			log_printf("Unknown protocol\n");
			return false;
		default:
			log_printf("WinInet error %d\n", inet_ret);
			return false;
		}
	}

	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE,
		&http_stat, &byte_ret, 0
	);
	log_printf("%d ", http_stat);
	if (http_stat != 200) {
		InternetCloseHandle(hFile);
		return false;
	}

	DWORD file_size;
	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH,
		&file_size, &byte_ret, 0
	);
	std::vector<uint8_t> buffer;
	if (!progressCallback(0, file_size)) {
		InternetCloseHandle(hFile);
		return false;
	}
	auto rem_size = file_size;

	while (rem_size) {
		DWORD read_size = 0;
		if (!InternetQueryDataAvailable(hFile, &read_size, 0, 0)) {
			read_size = rem_size;
		}
		if (read_size == 0) {
			log_printf("disconnected\n");
			InternetCloseHandle(hFile);
			return false;
		}
		buffer.resize(read_size);
		if (InternetReadFile(hFile, buffer.data(), read_size, &byte_ret)) {
			rem_size -= byte_ret;
			if (progressCallback(file_size - rem_size, file_size) == false || writeCallback(buffer.data(), read_size) != read_size) {
				InternetCloseHandle(hFile);
				return false;
			}
		}
		else {
			log_printf("\nReading error #%d! ", GetLastError());
			InternetCloseHandle(hFile);
			return false;
		}
	}

	InternetCloseHandle(hFile);
	return true;
}
