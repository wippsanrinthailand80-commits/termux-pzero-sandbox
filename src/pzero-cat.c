#define _GNU_SOURCE
#include "pzero_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>

static void usage(void) {
    fprintf(stderr, "Usage: pzero-cat [--offset N] [--limit N] [--max N] [--base64] PATH\n");
}

static const char b64tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int main(int argc, char **argv) {
    long offset = 0, limit = 65536, max = 1048576;
    int base64 = 0;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atol(argv[++i]);
        else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) limit = atol(argv[++i]);
        else if (strcmp(argv[i], "--max") == 0 && i + 1 < argc) max = atol(argv[++i]);
        else if (strcmp(argv[i], "--base64") == 0) base64 = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else path = argv[i];
    }
    if (!path) { usage(); return 1; }
    if (offset < 0) offset = 0;
    if (limit < 1) limit = 1;
    if (limit > max) limit = max;
    if (max < 1) max = 1;
    if (max > 16 * 1024 * 1024) max = 16 * 1024 * 1024;

    pzero_load_allowlists();
    char canon[PZERO_MAX_PATH];
    if (pzero_resolve_existing(path, canon, sizeof(canon)) != 0) {
        fprintf(stderr, "pzero-cat: cannot resolve: %s (%s)\n", path, strerror(errno));
        return 1;
    }
    if (!pzero_allowed_ro(canon)) {
        fprintf(stderr, "pzero-cat: access denied: %s\n", canon);
        return 1;
    }
    struct stat st;
    if (stat(canon, &st) != 0) {
        fprintf(stderr, "pzero-cat: stat failed: %s\n", strerror(errno));
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "pzero-cat: not a regular file: %s\n", canon);
        return 1;
    }

    int fd = open(canon, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "pzero-cat: open failed: %s\n", strerror(errno));
        return 1;
    }
    if (offset > 0) {
        if (lseek(fd, offset, SEEK_SET) < 0) {
            fprintf(stderr, "pzero-cat: seek failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
    }

    /* ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    char buf[65536];
    long total = 0;
    if (!base64) {
        while (total < limit) {
            size_t want = (size_t)(limit - total);
            if (want > sizeof(buf)) want = sizeof(buf);
            ssize_t r = read(fd, buf, want);
            if (r < 0) {
                if (errno == EINTR) continue;
                fprintf(stderr, "pzero-cat: read failed: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            if (r == 0) break;
            size_t w = 0;
            while (w < (size_t)r) {
                ssize_t wr = write(STDOUT_FILENO, buf + w, r - w);
                if (wr < 0) {
                    if (errno == EINTR) continue;
                    close(fd);
                    return 0; /* broken pipe */
                }
                w += wr;
            }
            total += r;
        }
    } else {
        /* base64 stream */
        char in[3];
        int inlen = 0;
        while (total < limit) {
            char c;
            ssize_t r = read(fd, &c, 1);
            if (r < 0) { if (errno == EINTR) continue; break; }
            if (r == 0) break;
            in[inlen++] = c;
            total++;
            if (inlen == 3) {
                int v = ((in[0] & 0xFF) << 16) | ((in[1] & 0xFF) << 8) | (in[2] & 0xFF);
                char out[4];
                out[0] = b64tab[(v >> 18) & 63];
                out[1] = b64tab[(v >> 12) & 63];
                out[2] = b64tab[(v >> 6) & 63];
                out[3] = b64tab[v & 63];
                write(STDOUT_FILENO, out, 4);
                inlen = 0;
            }
            if (total >= limit) break;
        }
        if (inlen > 0) {
            int v = (in[0] & 0xFF) << 16;
            if (inlen > 1) v |= (in[1] & 0xFF) << 8;
            char out[4];
            out[0] = b64tab[(v >> 18) & 63];
            out[1] = b64tab[(v >> 12) & 63];
            if (inlen > 1) out[2] = b64tab[(v >> 6) & 63]; else out[2] = '=';
            out[3] = '=';
            write(STDOUT_FILENO, out, 4);
        }
        write(STDOUT_FILENO, "\n", 1);
    }

    close(fd);
    return 0;
}
