#ifndef PZERO_COMMON_H
#define PZERO_COMMON_H

#include <stddef.h>

#define PZERO_MAX_PATH 4096
#define PZERO_MAX_LINE 8192

/* load allowlists from env/file/defaults; returns 0 */
int pzero_load_allowlists(void);

/* number of roots and arrays (read-only) */
size_t pzero_ro_count(void);
const char *pzero_ro_root(size_t i);
size_t pzero_rw_count(void);
const char *pzero_rw_root(size_t i);

int pzero_allowed_ro(const char *canon);
int pzero_allowed_rw(const char *canon);

/* resolve existing path to canonical; -1 on error */
int pzero_resolve_existing(const char *input, char *out, size_t outlen);

/* resolve path for writing; mkdirp creates missing parents; -1 on error */
int pzero_resolve_write(const char *input, char *out, size_t outlen, int mkdirp);

/* create directory tree; -1 on error */
int pzero_mkdir_p(const char *path);

/* json string escaper into buf; returns bytes written (excluding NUL) or -1 */
int pzero_json_escape(const char *src, char *dst, size_t dstlen);

/* safe basename checks */
int pzero_safe_name(const char *name);

#endif
