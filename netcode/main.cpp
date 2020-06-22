#include "repo_discovery.h"
#include "update.h"
#include "server.h"

int main()
{
    // TODO: move into thcrap
#ifdef USE_HTTP_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    //RepoDiscoverAtURL("https://mirrors.thpatch.net/nmlgc/"); // TODO: why is it the default??
    //if (RepoDiscoverAtURL("https://srv.thpatch.net/") != 0) {
    //    // TODO: log_printf (on every file)
    //    printf("Discovery from URL failed\n");
    //}
    //if (RepoDiscoverFromLocal() != 0) {
    //    printf("Discovery from local failed\n");
    //}

    std::unique_ptr<File> file = ServerCache::get().downloadFile("https://srv.thpatch.net/lang_en/patch.js");
    std::filesystem::create_directories("lang_en");
    file->write("lang_en/patch.js");

    patches_update({
        "lang_en"
    });

    // stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, NULL);
    // stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, NULL);
}
