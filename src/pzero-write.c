#define _GNU_SOURCE
#include "pzero_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>

static void usage(void) {
    fprintf(stderr, "Usage: pzero-write [--text STR] [--file SRC] [--stdin] [--mkdir] [--overwrite] [--append] [--mode MODE] PATH\n");
}

static int copy_fd_to_fd(int src, int dst) {
    char buf[65536];
    for (;;) {
        ssize_t r = read(src, buf, sizeof(buf));
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;
        size_t w = 0;
        while (w < (size_t)r) {
            ssize_t wr = write(dst, buf + w, r - w);
            if (wr < 0) { if (errno == EINTR) continue; return -1; }
            w += wr;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *path = NULL;
    const char *text = NULL;
    const char *srcfile = NULL;
    int mkdirp = 0;
    int overwrite = 0;
    int append = 0;
    unsigned int mode = 0644;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) { text = argv[++i]; }
        else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) { srcfile = argv[++i]; }
        else if (strcmp(argv[i], "--stdin") == 0) { /* read stdin */ }
        else if (strcmp(argv[i], "--mkdir") == 0) mkdirp = 1;
        else if (strcmp(argv[i], "--overwrite") == 0) overwrite = 1;
        else if (strcmp(argv[i], "--append") == 0) append = 1;
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) { mode = (unsigned int)strtoul(argv[++i], NULL, 8); }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else path = argv[i];
    }
    if (!path) { usage(); return 1; }
    /* default read stdin if no content source provided */

    pzero_load_allowlists();
    char canon[PZERO_MAX_PATH];
    if (pzero_resolve_write(path, canon, sizeof(canon), mkdirp) != 0) {
        fprintf(stderr, "pzero-write: cannot resolve: %s (%s)\n", path, strerror(errno));
        return 1;
    }
    if (!pzero_allowed_rw(canon)) {
        fprintf(stderr, "pzero-write: access denied: %s\n", canon);
        return 1;
    }

    /* check existing */
    struct stat st;
    int exists = (stat(canon, &st) == 0);
    if (exists && !overwrite && !append) {
        fprintf(stderr, "pzero-write: file exists, use --overwrite or --append: %s\n", canon);
        return 1;
    }
    if (exists && !S_ISREG(st.st_mode)) {
        fprintf(stderr, "pzero-write: not a regular file: %s\n", canon);
        return 1;
    }

    /* parent dir */
    char parent[PZERO_MAX_PATH];
    strncpy(parent, canon, sizeof(parent)-1);
    parent[sizeof(parent)-1] = '\0';
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0';
    else strcpy(parent, ".");

    /* temp file */
    char tmpl[PZERO_MAX_PATH];
    snprintf(tmpl, sizeof(tmpl), "%s/.pzero-write-XXXXXX", parent);
    int tfd = mkstemp(tmpl);
    if (tfd < 0) {
        fprintf(stderr, "pzero-write: cannot create temp: %s\n", strerror(errno));
        return 1;
    }
    fchmod(tfd, mode);

    /* if append and exists, copy existing first */
    if (append && exists) {
        int efd = open(canon, O_RDONLY | O_CLOEXEC);
        if (efd >= 0) {
            if (copy_fd_to_fd(efd, tfd) != 0) {
                fprintf(stderr, "pzero-write: copy existing failed: %s\n", strerror(errno));
                close(efd); close(tfd); unlink(tmpl);
                return 1;
            }
            close(efd);
        }
    }

    /* write content */
    if (text) {
        size_t len = strlen(text);
        size_t w = 0;
        while (w < len) {
            ssize_t wr = write(tfd, text + w, len - w);
            if (wr < 0) { if (errno == EINTR) continue; fprintf(stderr,"write failed: %s\n",strerror(errno)); close(tfd); unlink(tmpl); return 1; }
            w += wr;
        }
    } else if (srcfile) {
        /* resolve source */
        char srcanon[PZERO_MAX_PATH];
        if (pzero_resolve_existing(srcfile, srcanon, sizeof(srcanon)) != 0) {
            fprintf(stderr, "pzero-write: cannot resolve source: %s\n", srcfile);
            close(tfd); unlink(tmpl); return 1;
        }
        if (!pzero_allowed_ro(srcanon)) {
            fprintf(stderr, "pzero-write: source access denied: %s\n", srcanon);
            close(tfd); unlink(tmpl); return 1;
        }
        int sfd = open(srcanon, O_RDONLY | O_CLOEXEC);
        if (sfd < 0) {
            fprintf(stderr, "pzero-write: open source failed: %s\n", strerror(errno));
            close(tfd); unlink(tmpl); return 1;
        }
        if (copy_fd_to_fd(sfd, tfd) != 0) {
            fprintf(stderr, "pzero-write: copy failed: %s\n", strerror(errno));
            close(sfd); close(tfd); unlink(tmpl); return 1;
        }
        close(sfd);
    } else {
        /* stdin */
        if (copy_fd_to_fd(STDIN_FILENO, tfd) != 0) {
            fprintf(stderr, "pzero-write: stdin read failed: %s\n", strerror(errno));
            close(tfd); unlink(tmpl); return 1;
        }
    }

    /* fsync */
    fsync(tfd);
    close(tfd);

    /* atomic rename */
    if (rename(tmpl, canon) != 0) {
        fprintf(stderr, "pzero-write: rename failed: %s\n", strerror(errno));
        unlink(tmpl);
        return 1;
    }

    return 0;
}
