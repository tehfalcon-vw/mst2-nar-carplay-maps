#include "dcc_qnx.h"
#include <stdarg.h>

#define ISO_TX "/dev/transport_server/spawn/isoTX"
#define FD_CAP 16

struct iovec { void *iov_base; size_t iov_len; };
typedef int (*open_fn)(const char *, int, ...);
typedef int (*close_fn)(int);
typedef ssize_t (*write_fn)(int, const void *, size_t);
typedef ssize_t (*writev_fn)(int, const struct iovec *, int);

static volatile int custom_active;
static int iso_fds[FD_CAP];

static int same(const char *a, const char *b)
{
    size_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == b[i];
}

static void remember(int fd)
{
    int i;
    if (fd < 0) return;
    for (i = 0; i < FD_CAP; ++i) if (iso_fds[i] == fd) return;
    for (i = 0; i < FD_CAP; ++i) if (iso_fds[i] < 0) { iso_fds[i] = fd; return; }
}

static int tracked(int fd)
{
    int i;
    for (i = 0; i < FD_CAP; ++i) if (iso_fds[i] == fd) return 1;
    return 0;
}

static void forget(int fd)
{
    int i;
    for (i = 0; i < FD_CAP; ++i) if (iso_fds[i] == fd) iso_fds[i] = -1;
}

/* Called only by the in-process Stream-111 worker. A crashed worker therefore
 * cannot leave a persistent marker that suppresses stock navigation video. */
void dcc_video_gate_enable(int enabled) { custom_active = enabled != 0; }

int dcc_video_gate_fd(void)
{
    int i;
    for (i = 0; i < FD_CAP; ++i) if (iso_fds[i] >= 0) return iso_fds[i];
    return -1;
}

void dcc_video_gate_init(void)
{
    int i;
    custom_active = 0;
    for (i = 0; i < FD_CAP; ++i) iso_fds[i] = -1;
}

int open(const char *path, int flags, ...)
{
    open_fn next = (open_fn)dlsym(RTLD_NEXT, "open");
    int fd;
    if (flags & O_CREAT) {
        va_list ap;
        int mode;
        va_start(ap, flags); mode = va_arg(ap, int); va_end(ap);
        fd = next(path, flags, mode);
    } else fd = next(path, flags);
    if (same(path, ISO_TX)) remember(fd);
    return fd;
}

int close(int fd)
{
    close_fn next = (close_fn)dlsym(RTLD_NEXT, "close");
    forget(fd);
    return next(fd);
}

ssize_t write(int fd, const void *data, size_t length)
{
    write_fn next = (write_fn)dlsym(RTLD_NEXT, "write");
    if (custom_active && tracked(fd)) return (ssize_t)length;
    return next(fd, data, length);
}

ssize_t writev(int fd, const struct iovec *iov, int count)
{
    writev_fn next = (writev_fn)dlsym(RTLD_NEXT, "writev");
    size_t total = 0;
    int i;
    if (custom_active && tracked(fd)) {
        if (!iov || count < 0) return next(fd, iov, count);
        for (i = 0; i < count; ++i) total += iov[i].iov_len;
        return (ssize_t)total;
    }
    return next(fd, iov, count);
}
