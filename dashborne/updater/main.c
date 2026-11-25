#include "updater.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
#ifdef UPDATER_TEST
    const char *test = getenv("UPDATER_TEST_CARD");
    if (test) {
        card = test;
        static char s[PATH_CAP], l[PATH_CAP], c[PATH_CAP];
        path_join(s, card, "status");
        path_join(l, card, "list.tsv");
        path_join(c, card, "releases.json");
        status_path = s;
        list_path = l;
        cache_path = c;
    }
#endif
    if (setjmp(failure)) {
        char original[sizeof(error_text)];
        snprintf(original, sizeof(original), "%s", error_text);
        if (transaction_active) {
            transaction_active = 0;
            if (!setjmp(failure))
                recover_install();
        }
        status(original);
        fprintf(stderr, "%s\n", original);
        return 1;
    }
    if (argc < 2)
        fail("Usage: updater list | install RELEASE_ID | rollback | recover | confirm");
    mkdirs(work());
    char p[PATH_CAP];
    path_join(p, work(), "lock");
    int lock = open(p, O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB))
        fail("An update is already running or microSD is unavailable");
    int restart = 0;
    if (!strcmp(argv[1], "recover"))
        recover_install();
    else if (!strcmp(argv[1], "confirm"))
        confirm_install();
    else if (!strcmp(argv[1], "rollback")) {
        rollback_install();
        restart = 1;
    } else if (!strcmp(argv[1], "list")) {
#ifdef UPDATER_TEST
        if (!getenv("UPDATER_TEST_HTTP"))
#endif
            network_init();
        list_releases();
    } else if (!strcmp(argv[1], "install") && argc == 3) {
#ifdef UPDATER_TEST
        if (!getenv("UPDATER_TEST_HTTP"))
#endif
            network_init();
        download_release(argv[2]);
        restart = 1;
    }
#ifdef UPDATER_TEST
    else if (!strcmp(argv[1], "extract") && argc == 6)
        cJSON_Delete(extract_release(argv[2], argv[3], argv[4], argv[5]));
    else if (!strcmp(argv[1], "apply") && argc == 3)
        apply(argv[2]);
    else if (!strcmp(argv[1], "preserve") && argc == 3) {
        cJSON *m = read_manifest(argv[2]);
        preserve(argv[2], m);
        cJSON_Delete(m);
    } else if (!strcmp(argv[1], "url") && argc == 3) {
        if (!allowed_url(argv[2]))
            fail("URL rejected");
    } else if (!strcmp(argv[1], "text") && argc == 3) {
        char text[4];
        clean_text(text, sizeof(text), argv[2]);
        puts(text);
    }
#endif
    else
        fail("Unknown updater command");
    flock(lock, LOCK_UN);
    close(lock);
    if (restart) {
#ifdef UPDATER_TEST
        if (getenv("UPDATER_TEST_CARD"))
            return 0;
#endif
        status("Installed. Restarting into the boot menu...");
        sync();
        execl("/bin/setprop", "setprop", "ctl.restart", "zkswe", (char *)NULL);
        fail("Installed. Restart the DASH to finish");
    }
    return 0;
}
