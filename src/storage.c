#include "storage.h"
#include "config.h"
#include "util.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_BUF 32u

static int fsync_dir_of(const char *path)
{
    char dir[PATH_MAX_LEN];
    if (strlen(path) >= sizeof(dir)) { errno = ENAMETOOLONG; return -1; }
    strcpy(dir, path);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0'; else strcpy(dir, ".");
    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return -1;
    int rc = fsync(fd);
    close(fd);
    return rc;
}

int storage_save_u32(const char *path, uint32_t value)
{
    char tmp[PATH_MAX_LEN], num[NUM_BUF];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) { errno = ENAMETOOLONG; return -1; }
    int len = snprintf(num, sizeof(num), "%u\n", value);

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { LOG_ERRNO("open tmp"); return -1; }
    if (write(fd, num, (size_t)len) != len) { LOG_ERRNO("write"); close(fd); unlink(tmp); return -1; }
    if (fsync(fd) < 0) { LOG_ERRNO("fsync"); close(fd); unlink(tmp); return -1; }
    if (close(fd) < 0) { LOG_ERRNO("close"); unlink(tmp); return -1; }
    if (rename(tmp, path) < 0) { LOG_ERRNO("rename"); unlink(tmp); return -1; }
    if (fsync_dir_of(path) < 0) { LOG_ERRNO("fsync dir"); return -1; }
    return 0;
}

int storage_load_u32(const char *path, uint32_t *value)
{
    char buf[NUM_BUF];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) { errno = EINVAL; return -1; }
    buf[n] = '\0';

    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(buf, &end, 10);
    if (errno != 0 || end == buf || (*end != '\n' && *end != '\0') || v > UINT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    *value = (uint32_t)v;
    return 0;
}
