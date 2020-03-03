#include "netcode.h"

int main()
{
    // TODO: move into thcrap
    curl_global_init(CURL_GLOBAL_DEFAULT);

    //RepoDiscoverAtURL("https://mirrors.thpatch.net/nmlgc/"); // TODO: why is it the default??
    if (RepoDiscoverAtURL("https://srv.thpatch.net/") != 0) {
        // TODO: log_printf (on every file)
        printf("Discovery from URL failed\n");
    }
    //if (RepoDiscoverFromLocal() != 0) {
    //    printf("Discovery from local failed\n");
    //}
}
