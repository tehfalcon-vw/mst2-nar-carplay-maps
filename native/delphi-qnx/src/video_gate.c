#include "dcc_qnx.h"
#include "dcc_ts.h"
#include "dcc_video_slot.h"
#include <stdarg.h>

#define ISO_TX "/dev/transport_server/spawn/isoTX"
#define VIDEO_SLOT "/dev/shmem/mst2_nar_alt111"
#define FD_CAP 16
#define TS_CAPACITY 260000u
#define STALE_POLLS 400u

struct iovec { void *iov_base; size_t iov_len; };
typedef int (*open_fn)(const char *, int, ...);
typedef int (*close_fn)(int);
typedef ssize_t (*write_fn)(int, const void *, size_t);
typedef ssize_t (*writev_fn)(int, const struct iovec *, int);

static volatile int custom_active;
static int iso_fds[FD_CAP];
static uint8_t access_unit[DCC_VIDEO_SLOT_CAPACITY];
static uint8_t transport_stream[TS_CAPACITY];

struct ts_capture { size_t used; };

static int capture_packet(const uint8_t packet[DCC_TS_PACKET_SIZE], void *opaque)
{
    struct ts_capture *capture = (struct ts_capture *)opaque;
    if (capture->used + DCC_TS_PACKET_SIZE > sizeof(transport_stream)) return -1;
    memcpy(transport_stream + capture->used, packet, DCC_TS_PACKET_SIZE);
    capture->used += DCC_TS_PACKET_SIZE;
    return 0;
}

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

static const struct dcc_video_slot *map_video_slot(void)
{
    int fd = open(VIDEO_SLOT, O_RDONLY);
    off_t size;
    void *mapping;
    if (fd < 0) return NULL;
    size = lseek(fd, 0, SEEK_END);
    if (size < (off_t)sizeof(struct dcc_video_slot)) { close(fd); return NULL; }
    mapping = mmap(NULL, sizeof(struct dcc_video_slot), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    return mapping == MAP_FAILED ? NULL : (const struct dcc_video_slot *)mapping;
}

static void *video_worker(void *unused)
{
    const struct dcc_video_slot *slot = NULL;
    write_fn real_write = (write_fn)dlsym(RTLD_NEXT, "write");
    struct dcc_ts_mux mux;
    uint32_t sequence = 0, flags = 0, stale = 0;
    uint64_t pts90k = 0;
    size_t length = 0;
    int have_idr = 0;
    (void)unused;
    dcc_ts_init(&mux);
    if (!real_write) return NULL;
    for (;;) {
        int result, fd;
        struct ts_capture capture;
        if (!slot) {
            slot = map_video_slot();
            if (!slot) { delay(200); continue; }
        }
        result = dcc_video_slot_read(slot, &sequence, access_unit, sizeof(access_unit),
                                     &length, &pts90k, &flags);
        if (result < 0) {
            custom_active = 0; have_idr = 0; sequence = 0; slot = NULL;
            delay(200); continue;
        }
        if (result == 0) {
            if (custom_active && ++stale > STALE_POLLS) {
                custom_active = 0; have_idr = 0; dcc_ts_init(&mux);
            }
            delay(5); continue;
        }
        stale = 0;
        if (!(flags & DCC_VIDEO_FLAG_ACTIVE)) {
            custom_active = 0; have_idr = 0; dcc_ts_init(&mux); continue;
        }
        if (!have_idr && !(flags & DCC_VIDEO_FLAG_IDR)) continue;
        if (flags & DCC_VIDEO_FLAG_IDR) have_idr = 1;
        fd = dcc_video_gate_fd();
        if (fd < 0) continue;
        capture.used = 0;
        if (dcc_ts_push_annexb(&mux, access_unit, length, pts90k,
                               capture_packet, &capture) || !capture.used) {
            custom_active = 0; have_idr = 0; dcc_ts_init(&mux); continue;
        }
        custom_active = 1;
        if (real_write(fd, transport_stream, capture.used) != (ssize_t)capture.used) {
            custom_active = 0; have_idr = 0; dcc_ts_init(&mux);
        }
    }
}

void dcc_video_gate_init(void)
{
    int i;
    pthread_t worker;
    custom_active = 0;
    for (i = 0; i < FD_CAP; ++i) iso_fds[i] = -1;
    if (pthread_create(&worker, NULL, video_worker, NULL) == 0) pthread_detach(worker);
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
