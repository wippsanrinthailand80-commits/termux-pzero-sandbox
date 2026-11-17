#define _GNU_SOURCE
#include "pzero_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>

static void usage(void) {
    fprintf(stderr, "Usage: pzero-exec [--timeout SEC] [--cwd DIR] [--max-output N] [--] COMMAND [ARGS...]\n");
}

static char **bin_dirs(void) {
    static char *dirs[16];
    static int nd = -1;
    if (nd >= 0) return dirs;
    nd = 0;
    const char *env = getenv("PZERO_BIN");
    if (env) {
        char *copy = strdup(env);
        char *save = NULL;
        char *tok = strtok_r(copy, ":", &save);
        while (tok && nd < 15) dirs[nd++] = strdup(tok), tok = strtok_r(NULL, ":", &save);
    }
    if (nd == 0) {
        const char *prefix = getenv("PREFIX");
        if (prefix) { char b[PZERO_MAX_PATH]; snprintf(b,sizeof(b),"%s/bin",prefix); dirs[nd++]=strdup(b); }
        dirs[nd++] = strdup("/data/data/com.termux/files/usr/bin");
        dirs[nd++] = strdup("/bin");
        dirs[nd++] = strdup("/usr/bin");
    }
    dirs[nd] = NULL;
    return dirs;
}

static int resolve_executable(const char *cmd, char *out, size_t outlen) {
    if (strchr(cmd, '/')) {
        if (pzero_resolve_existing(cmd, out, outlen) != 0) return -1;
        struct stat st;
        if (stat(out, &st) != 0 || !S_ISREG(st.st_mode)) return -1;
        if (access(out, X_OK) != 0) return -1;
        return 0;
    }
    char **dirs = bin_dirs();
    for (int i = 0; dirs[i]; i++) {
        char cand[PZERO_MAX_PATH];
        snprintf(cand, sizeof(cand), "%s/%s", dirs[i], cmd);
        if (access(cand, X_OK) == 0 && realpath(cand, out)) return 0;
    }
    return -1;
}

static void set_limits(int cpu_seconds) {
    struct rlimit rl;
    rl.rlim_cur = cpu_seconds; rl.rlim_max = cpu_seconds;
    setrlimit(RLIMIT_CPU, &rl);
    rl.rlim_cur = 64*1024*1024; rl.rlim_max = 64*1024*1024;
    setrlimit(RLIMIT_FSIZE, &rl);
    rl.rlim_cur = 0; rl.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rl);
    rl.rlim_cur = 256; rl.rlim_max = 256;
    setrlimit(RLIMIT_NOFILE, &rl);
}


static int elf_machine(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char e[20];
    if (fread(e, 1, 20, f) != 20) { fclose(f); return -1; }
    fclose(f);
    if (e[0] != 0x7f || e[1] != 'E' || e[2] != 'L' || e[3] != 'F') return -1;
    return (int)(e[18] | (e[19] << 8));
}

int main(int argc, char **argv) {
    int timeout = 30;
    const char *cwd = NULL;
    int maxout = 1024 * 1024;
    int cmd_start = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) { cmd_start = i + 1; break; }
        if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) { timeout = atoi(argv[++i]); continue; }
        if (strcmp(argv[i], "--cwd") == 0 && i + 1 < argc) { cwd = argv[++i]; continue; }
        if (strcmp(argv[i], "--max-output") == 0 && i + 1 < argc) { maxout = atoi(argv[++i]); continue; }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        cmd_start = i;
        break;
    }
    if (cmd_start < 0 || cmd_start >= argc) { usage(); return 1; }
    if (timeout < 1) timeout = 1;
    if (timeout > 300) timeout = 300;
    if (maxout < 1024) maxout = 1024;

    (void)maxout;
    pzero_load_allowlists();

    char exe[PZERO_MAX_PATH];
    if (resolve_executable(argv[cmd_start], exe, sizeof(exe)) != 0) {
        fprintf(stderr, "pzero-exec: command not found or not allowed: %s\n", argv[cmd_start]);
        return 127;
    }

    /* Detect x86/x86_64 binaries and run via QEMU user-mode if available. */
    char orig_exe[PZERO_MAX_PATH];
    strcpy(orig_exe, exe);
    int mach = elf_machine(exe);
    const char *qemu_name = NULL;
    if (mach == 3) qemu_name = "qemu-i386";
    else if (mach == 62) qemu_name = "qemu-x86_64";
    if (qemu_name) {
        char qpath[PZERO_MAX_PATH];
        if (resolve_executable(qemu_name, qpath, sizeof(qpath)) != 0) {
            fprintf(stderr, "pzero-exec: %s binary requires %s, but it is not installed\n", argv[cmd_start], qemu_name);
            return 127;
        }
        strcpy(exe, qpath);
    }

    if (cwd) {
        char cwda[PZERO_MAX_PATH];
        if (pzero_resolve_existing(cwd, cwda, sizeof(cwda)) != 0) {
            fprintf(stderr, "pzero-exec: cannot resolve cwd: %s\n", cwd);
            return 1;
        }
        if (!pzero_allowed_ro(cwda)) {
            fprintf(stderr, "pzero-exec: cwd access denied: %s\n", cwda);
            return 1;
        }
        if (chdir(cwda) != 0) {
            fprintf(stderr, "pzero-exec: chdir failed: %s\n", strerror(errno));
            return 1;
        }
    }

    const char *home = getenv("HOME");

    /* Reset environment: keep only safe variables. */
    clearenv();
    setenv("PATH", "/data/data/com.termux/files/usr/bin:/bin:/usr/bin", 1);
    setenv("HOME", home ? home : "/tmp", 1);
    setenv("TMPDIR", "/tmp", 1);
    setenv("LANG", "C.UTF-8", 1);
    setenv("ANDROID_ROOT", "/system", 1);
    setenv("ANDROID_DATA", "/data", 1);

    set_limits(timeout);

    /* Wall-clock timeout via alarm: alarm() survives execve on this platform. */
    alarm((unsigned int)timeout);

    /* Build argv: replace command name with resolved path. */
    int nargs = argc - cmd_start;
    int use_qemu = (strcmp(exe, orig_exe) != 0);
    int total = use_qemu ? nargs + 1 : nargs;
    char **cargv = calloc(total + 1, sizeof(char *));
    if (!cargv) { perror("calloc"); return 1; }
    if (use_qemu) {
        cargv[0] = exe; /* qemu */
        cargv[1] = orig_exe; /* target binary */
        for (int i = 1; i < nargs; i++) cargv[1 + i] = argv[cmd_start + i];
        cargv[total] = NULL;
    } else {
        cargv[0] = argv[cmd_start];
        for (int i = 1; i < nargs; i++) cargv[i] = argv[cmd_start + i];
        cargv[nargs] = NULL;
    }
    execv(exe, cargv);
    fprintf(stderr, "pzero-exec: exec failed: %s\n", strerror(errno));
    return 127;
}
