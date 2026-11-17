#define _GNU_SOURCE
#include "pzero_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>

static char **ro_roots = NULL;
static size_t ro_n = 0;
static char **rw_roots = NULL;
static size_t rw_n = 0;
static int loaded = 0;

static void add_root(char ***arr, size_t *n, const char *path) {
    if (!path || !*path) return;
    char buf[PZERO_MAX_PATH];
    if (realpath(path, buf)) {
        /* store canonical */
    } else {
        /* store as-is if not exists */
        strncpy(buf, path, sizeof(buf)-1);
        buf[sizeof(buf)-1] = '\0';
    }
    /* avoid duplicates */
    for (size_t i = 0; i < *n; i++) {
        if (strcmp((*arr)[i], buf) == 0) return;
    }
    char **tmp = realloc(*arr, (*n + 1) * sizeof(char *));
    if (!tmp) return;
    *arr = tmp;
    (*arr)[*n] = strdup(buf);
    if ((*arr)[*n]) (*n)++;
}

static void parse_colon(const char *s, char ***arr, size_t *n) {
    if (!s) return;
    char *copy = strdup(s);
    if (!copy) return;
    char *save = NULL;
    char *tok = strtok_r(copy, ":", &save);
    while (tok) {
        add_root(arr, n, tok);
        tok = strtok_r(NULL, ":", &save);
    }
    free(copy);
}

static void parse_allowlist_file(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return;
    char line[PZERO_MAX_PATH];
    while (fgets(line, sizeof(line), f)) {
        /* strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        /* skip comments/empty */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\0') continue;
        char path[PZERO_MAX_PATH];
        char mode[8] = "ro";
        if (sscanf(p, "%4095s %7s", path, mode) < 1) continue;
        if (strcmp(mode, "rw") == 0) add_root(&rw_roots, &rw_n, path);
        else add_root(&ro_roots, &ro_n, path);
        /* ro entry also readable */
        add_root(&ro_roots, &ro_n, path);
    }
    fclose(f);
}

static char *default_config_path(void) {
    static char buf[PZERO_MAX_PATH];
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    snprintf(buf, sizeof(buf), "%s/.config/pzero/allowlist", home);
    return buf;
}

int pzero_load_allowlists(void) {
    if (loaded) return 0;
    loaded = 1;

    const char *envro = getenv("PZERO_ALLOW_RO");
    const char *envrw = getenv("PZERO_ALLOW_RW");

    if (envro) parse_colon(envro, &ro_roots, &ro_n);
    if (envrw) parse_colon(envrw, &rw_roots, &rw_n);

    if (!envro && !envrw) {
        parse_allowlist_file(default_config_path());
    }

    /* defaults */
    if (ro_n == 0) {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        add_root(&ro_roots, &ro_n, home);
        add_root(&ro_roots, &ro_n, "/tmp");
        add_root(&ro_roots, &ro_n, "/usr");
        add_root(&ro_roots, &ro_n, "/bin");
        add_root(&ro_roots, &ro_n, "/etc");
        add_root(&ro_roots, &ro_n, "/lib");
        add_root(&ro_roots, &ro_n, "/share");
        add_root(&ro_roots, &ro_n, "/var");
    }
    if (rw_n == 0) {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        add_root(&rw_roots, &rw_n, home);
        add_root(&rw_roots, &rw_n, "/tmp");
    }

    /* rw implies ro */
    for (size_t i = 0; i < rw_n; i++) add_root(&ro_roots, &ro_n, rw_roots[i]);

    return 0;
}

size_t pzero_ro_count(void) { pzero_load_allowlists(); return ro_n; }
const char *pzero_ro_root(size_t i) { pzero_load_allowlists(); return (i < ro_n) ? ro_roots[i] : NULL; }
size_t pzero_rw_count(void) { pzero_load_allowlists(); return rw_n; }
const char *pzero_rw_root(size_t i) { pzero_load_allowlists(); return (i < rw_n) ? rw_roots[i] : NULL; }

static int under_list(const char *canon, char **arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        size_t len = strlen(arr[i]);
        if (strcmp(canon, arr[i]) == 0) return 1;
        if (strncmp(canon, arr[i], len) == 0 && canon[len] == '/') return 1;
    }
    return 0;
}

int pzero_allowed_ro(const char *canon) {
    pzero_load_allowlists();
    if (!canon) return 0;
    return under_list(canon, ro_roots, ro_n);
}
int pzero_allowed_rw(const char *canon) {
    pzero_load_allowlists();
    if (!canon) return 0;
    return under_list(canon, rw_roots, rw_n);
}

int pzero_resolve_existing(const char *input, char *out, size_t outlen) {
    if (!input || !*input || !out || outlen == 0) return -1;
    char path[PZERO_MAX_PATH];
    if (input[0] != '/') {
        char cwd[PZERO_MAX_PATH];
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        if (strlen(cwd) + strlen(input) + 2 > sizeof(path)) return -1;
        snprintf(path, sizeof(path), "%s/%s", cwd, input);
    } else {
        if (strlen(input) >= sizeof(path)) return -1;
        strncpy(path, input, sizeof(path)-1);
        path[sizeof(path)-1] = '\0';
    }
    if (!realpath(path, out)) return -1;
    /* ensure within max */
    if (strlen(out) >= outlen) return -1;
    return 0;
}

int pzero_mkdir_p(const char *path) {
    if (!path || path[0] != '/') return -1;
    char tmp[PZERO_MAX_PATH];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    /* remove trailing slash */
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len-1] == '/') { tmp[--len] = '\0'; }

    char cur[PZERO_MAX_PATH];
    cur[0] = '/';
    cur[1] = '\0';
    char *p = tmp + 1;
    while (*p) {
        char *slash = strchr(p, '/');
        size_t comp_len;
        if (slash) comp_len = (size_t)(slash - p);
        else comp_len = strlen(p);
        if (comp_len == 0) { p++; continue; }
        if (comp_len >= sizeof(cur) - strlen(cur) - 1) return -1;
        strncat(cur, p, comp_len);
        /* create */
        if (mkdir(cur, 0755) != 0 && errno != EEXIST) return -1;
        /* verify not symlink escape: realpath check done by caller */
        strcat(cur, "/");
        if (slash) p = slash + 1;
        else break;
    }
    return 0;
}

int pzero_resolve_write(const char *input, char *out, size_t outlen, int mkdirp) {
    if (!input || !*input || !out || outlen == 0) return -1;
    char path[PZERO_MAX_PATH];
    if (input[0] != '/') {
        char cwd[PZERO_MAX_PATH];
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        if (strlen(cwd) + strlen(input) + 2 > sizeof(path)) return -1;
        snprintf(path, sizeof(path), "%s/%s", cwd, input);
    } else {
        if (strlen(input) >= sizeof(path)) return -1;
        strncpy(path, input, sizeof(path)-1);
        path[sizeof(path)-1] = '\0';
    }

    /* if exists, canonicalize */
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (!realpath(path, out)) return -1;
        if (strlen(out) >= outlen) return -1;
        return 0;
    }

    /* split last component */
    char tmp[PZERO_MAX_PATH];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    /* remove trailing slash */
    size_t tlen = strlen(tmp);
    while (tlen > 1 && tmp[tlen-1] == '/') tmp[--tlen] = '\0';

    char *slash = strrchr(tmp, '/');
    if (!slash) return -1; /* no parent */
    char base[PZERO_MAX_PATH];
    strncpy(base, slash + 1, sizeof(base)-1);
    base[sizeof(base)-1] = '\0';
    *slash = '\0';
    char *parent = tmp;

    if (base[0] == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) return -1;
    if (strchr(base, '/') != NULL) return -1;

    /* create parent if needed */
    if (mkdirp) {
        /* ensure parent exists */
        struct stat pst;
        if (stat(parent, &pst) != 0) {
            if (pzero_mkdir_p(parent) != 0) return -1;
        }
    }

    char parentcanon[PZERO_MAX_PATH];
    if (!realpath(parent, parentcanon)) return -1;

    snprintf(out, outlen, "%s/%s", parentcanon, base);
    if (strlen(out) >= outlen) return -1;
    return 0;
}

int pzero_json_escape(const char *src, char *dst, size_t dstlen) {
    if (!dst || dstlen == 0) return -1;
    size_t di = 0;
    if (!src) src = "";
    for (const char *p = src; *p; p++) {
        unsigned char c = (unsigned char)*p;
        const char *esc = NULL;
        char buf[8];
        if (c == '"') esc = "\\\"";
        else if (c == '\\') esc = "\\\\";
        else if (c == '\n') esc = "\\n";
        else if (c == '\r') esc = "\\r";
        else if (c == '\t') esc = "\\t";
        else if (c < 0x20) {
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            esc = buf;
        }
        if (esc) {
            size_t el = strlen(esc);
            if (di + el + 1 > dstlen) return -1;
            memcpy(dst + di, esc, el);
            di += el;
        } else {
            if (di + 2 > dstlen) return -1;
            dst[di++] = (char)c;
        }
    }
    if (di + 1 > dstlen) return -1;
    dst[di] = '\0';
    return (int)di;
}

int pzero_safe_name(const char *name) {
    if (!name || !*name) return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    if (strchr(name, '/') != NULL) return 0;
    if (strchr(name, '\0') == NULL) return 0;
    return 1;
}
