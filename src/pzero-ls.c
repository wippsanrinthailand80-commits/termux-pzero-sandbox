#define _GNU_SOURCE
#include "pzero_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>

static void usage(void) {
    fprintf(stderr, "Usage: pzero-ls [--json] [-a] [--max N] PATH\n");
}

typedef struct { char name[256]; } entry_t;

static int cmp_name(const void *a, const void *b) {
    return strcmp(((const entry_t *)a)->name, ((const entry_t *)b)->name);
}

int main(int argc, char **argv) {
    int json = 0, show_all = 0;
    long max = 10000;
    const char *path = ".";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json = 1;
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) show_all = 1;
        else if (strcmp(argv[i], "--max") == 0 && i + 1 < argc) { max = atol(argv[++i]); }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else path = argv[i];
    }
    if (max < 1) max = 1;
    if (max > 100000) max = 100000;

    pzero_load_allowlists();
    char canon[PZERO_MAX_PATH];
    if (pzero_resolve_existing(path, canon, sizeof(canon)) != 0) {
        fprintf(stderr, "pzero-ls: cannot resolve path: %s (%s)\n", path, strerror(errno));
        return 1;
    }
    if (!pzero_allowed_ro(canon)) {
        fprintf(stderr, "pzero-ls: access denied: %s\n", canon);
        return 1;
    }
    struct stat st;
    if (lstat(canon, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "pzero-ls: not a directory: %s\n", canon);
        return 1;
    }

    DIR *d = opendir(canon);
    if (!d) {
        fprintf(stderr, "pzero-ls: cannot open: %s (%s)\n", canon, strerror(errno));
        return 1;
    }

    entry_t *entries = calloc(max, sizeof(entry_t));
    if (!entries) { closedir(d); return 1; }
    size_t n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < (size_t)max) {
        if (!show_all && de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (!pzero_safe_name(de->d_name)) continue;
        strncpy(entries[n].name, de->d_name, 255);
        entries[n].name[255] = '\0';
        n++;
    }
    closedir(d);
    qsort(entries, n, sizeof(entry_t), cmp_name);

    if (json) {
        printf("{\"path\":\"");
        char esc[PZERO_MAX_PATH * 2];
        pzero_json_escape(canon, esc, sizeof(esc));
        printf("%s\",\"entries\":[", esc);
        for (size_t i = 0; i < n; i++) {
            char full[PZERO_MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", canon, entries[i].name);
            struct stat est;
            if (lstat(full, &est) != 0) continue;
            const char *type = "other";
            if (S_ISDIR(est.st_mode)) type = "dir";
            else if (S_ISREG(est.st_mode)) type = "file";
            else if (S_ISLNK(est.st_mode)) type = "symlink";
            else if (S_ISFIFO(est.st_mode)) type = "fifo";
            if (i) printf(",");
            printf("{\"name\":\"");
            char nesc[512];
            pzero_json_escape(entries[i].name, nesc, sizeof(nesc));
            printf("%s\",\"type\":\"%s\",\"size\":%ld}", nesc, type, (long)est.st_size);
        }
        printf("]}\n");
    } else {
        for (size_t i = 0; i < n; i++) {
            char full[PZERO_MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", canon, entries[i].name);
            struct stat est;
            if (lstat(full, &est) != 0) continue;
            char type = '-';
            if (S_ISDIR(est.st_mode)) type = 'd';
            else if (S_ISLNK(est.st_mode)) type = 'l';
            printf("%c %10ld  %s\n", type, (long)est.st_size, entries[i].name);
        }
    }

    free(entries);
    return 0;
}
