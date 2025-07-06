#include "thcrap.h"
#include <windows.h>
#include <stdexcept>
#include <vector>
#include "http_wininet.h"

WininetHandle::WininetHandle()
{
    DWORD ignore = 1;
    // DWORD timeout = 500;

    // Format according to RFC 7231, section 5.5.3
    std::string agent = std::string(PROJECT_NAME_SHORT) + "/" + PROJECT_VERSION_STRING + " (" + windows_version() + ")";

    this->internet = InternetOpenU(agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (this->internet == nullptr) {
		log_printf("Could not initialize HTTP library. No internet access?\n");
        return ;
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

WininetHandle::WininetHandle(WininetHandle&& other)
    : internet(other.internet)
{
    other.internet = nullptr;
}

WininetHandle::~WininetHandle()
{
    if (this->internet) {
        InternetCloseHandle(this->internet);
    }
}

class ScopedHInternet
{
private:
	HINTERNET hInternet;

public:
	ScopedHInternet(HINTERNET hInternet)
		: hInternet(hInternet)
	{}
	~ScopedHInternet()
	{
		if (this->hInternet) {
			InternetCloseHandle(this->hInternet);
		}
	}
	operator HINTERNET()
	{
		return this->hInternet;
	}

	ScopedHInternet(const ScopedHInternet& src) = delete;
	ScopedHInternet(ScopedHInternet&& src) = delete;
	const ScopedHInternet& operator=(const ScopedHInternet& src) = delete;
	const ScopedHInternet& operator=(ScopedHInternet&& src) = delete;
};

HttpStatus WininetHandle::download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback)
{
	DWORD byte_ret = sizeof(DWORD);
	DWORD http_stat = 0;

    if (this->internet == nullptr) {
        // Could be an invalid argument, using an HttpWininet which
        // have been moved into another object.
        // But most probably the InternetOpen in the constructor failed,
        // so we already displayed an error and we just want to leave.
        return HttpStatus::makeSystemError(0, "Wininet is not initialized");
    }

	ScopedHInternet hFile = InternetOpenUrlA(
		this->internet, url.c_str(), NULL, 0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_KEEP_CONNECTION, 0
	);
	if (!hFile) {
		// TODO: We should use FormatMessage() for both coverage and i18n
		// reasons here, but its messages are way too verbose for my taste.
		// So let's wait with that until this code is used in conjunction
		// with a GUI, if at all.
		DWORD inet_ret = GetLastError();
		const char *msg;
		switch (inet_ret) {
		case ERROR_INTERNET_NAME_NOT_RESOLVED:
			msg = "could not resolve hostname";
			break;
		case ERROR_INTERNET_CANNOT_CONNECT:
			msg = "connection refused";
			break;
		case ERROR_INTERNET_TIMEOUT:
			msg = "timed out";
			break;
		case ERROR_INTERNET_UNRECOGNIZED_SCHEME:
			msg = "unknown protocol";
			break;
		default:
			msg = "WinInet error";
			break;
		}
		return HttpStatus::makeSystemError(inet_ret, msg);
	}

	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE,
		&http_stat, &byte_ret, 0
	);
	if (http_stat != 200) {
		return HttpStatus::makeNetworkError(http_stat);
	}

	DWORD file_size;
	HttpQueryInfo(hFile, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH,
		&file_size, &byte_ret, 0
	);
	std::vector<uint8_t> buffer;
	if (!progressCallback(0, file_size)) {
		return HttpStatus::makeCancelled();
	}
	auto rem_size = file_size;

	while (rem_size) {
		DWORD read_size = 0;
		if (!InternetQueryDataAvailable(hFile, &read_size, 0, 0)) {
			read_size = rem_size;
		}
		if (read_size == 0) {
			return HttpStatus::makeSystemError(0, "disconnected");
		}
		buffer.resize(read_size);
		if (InternetReadFile(hFile, buffer.data(), read_size, &byte_ret) == FALSE) {
			return HttpStatus::makeSystemError(GetLastError(), "reading error");
		}
		rem_size -= byte_ret;
		if (progressCallback(file_size - rem_size, file_size) == false) {
			return HttpStatus::makeCancelled();
		}
        if (writeCallback(buffer.data(), read_size) != read_size) {
			return HttpStatus::makeSystemError(GetLastError(), "writing error");
		}
	}

	return HttpStatus::makeOk();
}
