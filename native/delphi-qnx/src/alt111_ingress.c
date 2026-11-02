#include "dcc_alt111_ingress.h"
#include "dcc_qnx.h"
#include "dcc_screen.h"
#include "dcc_screen_session.h"

#define VIDEO_SLOT_PATH "/dev/shmem/mst2_nar_alt111"

typedef void (*derive_fn)(const void *, size_t, uint64_t, uint8_t *, uint8_t *);
typedef int (*ctr_init_fn)(void *, const uint8_t *, const uint8_t *);
typedef int (*ctr_update_fn)(void *, const void *, size_t, void *);
typedef void (*ctr_final_fn)(void *);

union cipher_storage { uint64_t align; uint8_t bytes[512]; };

static struct dcc_video_slot *slot;
static struct dcc_screen_parser parser;
static struct dcc_screen_session session;
static union cipher_storage cipher;
static derive_fn derive_key;
static ctr_init_fn ctr_init;
static ctr_update_fn ctr_update;
static ctr_final_fn ctr_final;
static uint8_t body[DCC_VIDEO_SLOT_CAPACITY];
static uint8_t annexb[DCC_VIDEO_SLOT_CAPACITY];
static uint64_t packet_now_ms;
static uint64_t packet_pts90k;
static int cipher_valid;

static int decrypt_frame(uint8_t *bytes, size_t length, void *opaque)
{
    (void)opaque;
    if (!cipher_valid || !ctr_update) return -1;
    return ctr_update(&cipher, bytes, length, bytes);
}

static int on_packet(const struct dcc_screen_header *header, uint8_t *bytes,
                     size_t length, void *opaque)
{
    int result;
    (void)opaque;
    result = dcc_screen_session_packet(&session, header, bytes, length,
                                       packet_now_ms, packet_pts90k);
    if (header->opcode == DCC_SCREEN_VIDEO_FRAME) packet_pts90k += 3000u;
    return result < 0 ? -1 : 0;
}

static int map_slot(void)
{
    int fd;
    void *mapping;
    if (slot) return 0;
    fd = open(VIDEO_SLOT_PATH, O_RDWR | O_CREAT, 0666);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)sizeof(struct dcc_video_slot))) {
        close(fd);
        return -1;
    }
    mapping = mmap(NULL, sizeof(struct dcc_video_slot),
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) return -1;
    slot = (struct dcc_video_slot *)mapping;
    dcc_video_slot_init(slot);
    return 0;
}

static int resolve_crypto(void)
{
    derive_key = (derive_fn)dlsym(RTLD_DEFAULT,
                                  "AirPlay_DeriveAESKeySHA512ForScreen");
    ctr_init = (ctr_init_fn)dlsym(RTLD_DEFAULT, "AES_CTR_Init");
    ctr_update = (ctr_update_fn)dlsym(RTLD_DEFAULT, "AES_CTR_Update");
    ctr_final = (ctr_final_fn)dlsym(RTLD_DEFAULT, "AES_CTR_Final");
    return derive_key && ctr_init && ctr_update && ctr_final ? 0 : -1;
}

int dcc_alt111_ingress_begin(const uint8_t master_key[16],
                             uint64_t stream_connection_id)
{
    uint8_t key[16], iv[16];
    if (!master_key || !stream_connection_id || map_slot() || resolve_crypto())
        return -1;
    dcc_alt111_ingress_stop();
    derive_key(master_key, 16, stream_connection_id, key, iv);
    memset(&cipher, 0, sizeof(cipher));
    if (ctr_init(&cipher, key, iv)) {
        memset(key, 0, sizeof(key));
        memset(iv, 0, sizeof(iv));
        return -1;
    }
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    cipher_valid = 1;
    dcc_screen_session_init(&session, slot, annexb, sizeof(annexb),
                            decrypt_frame, NULL, 2000u);
    dcc_screen_parser_init(&parser, body, sizeof(body), on_packet, NULL);
    return 0;
}

int dcc_alt111_ingress_feed(const uint8_t *bytes, size_t length,
                            uint64_t now_ms, uint64_t first_pts90k)
{
    if (!cipher_valid) return -1;
    packet_now_ms = now_ms;
    packet_pts90k = first_pts90k;
    return dcc_screen_parser_feed(&parser, bytes, length);
}

int dcc_alt111_ingress_poll(uint64_t now_ms)
{
    if (!cipher_valid) return 0;
    return dcc_screen_session_poll(&session, now_ms);
}

void dcc_alt111_ingress_stop(void)
{
    if (slot) dcc_screen_session_stop(&session);
    if (cipher_valid && ctr_final) ctr_final(&cipher);
    memset(&cipher, 0, sizeof(cipher));
    cipher_valid = 0;
    dcc_screen_parser_reset(&parser);
}
