#include "dcc_alt111_ingress.h"
#include "dcc_qnx.h"
#include "dcc_runtime_hook.h"

typedef const void *CFTypeRef;
typedef const void *CFStringRef;
typedef const void *CFDictionaryRef;
typedef void *CFMutableDictionaryRef;
typedef const void *CFArrayRef;
typedef void *CFMutableArrayRef;
typedef long CFIndex;
typedef int32_t OSStatus;
typedef unsigned char Boolean;
typedef void *AirPlayReceiverSessionRef;

#define CF_UTF8 0x08000100u
#define STREAM_ALT 111
#define AF_INET_VALUE 2
#define SOCK_STREAM_VALUE 1
#define IPPROTO_TCP_VALUE 6
#define ALT_PORT 7001
#define ALT_BUFFER 524288

typedef CFDictionaryRef (*server_info_fn)(AirPlayReceiverSessionRef, CFArrayRef,
                                          uint8_t *, OSStatus *);
typedef OSStatus (*setup_fn)(AirPlayReceiverSessionRef, CFDictionaryRef,
                             CFDictionaryRef *);
typedef void (*teardown_fn)(AirPlayReceiverSessionRef, CFDictionaryRef,
                            OSStatus, Boolean *);
typedef OSStatus (*cbc_init_fn)(void *, const uint8_t *, const uint8_t *, Boolean);
typedef OSStatus (*server_socket_fn)(int, int, int, int, int *, int, int *);

static struct dcc_p0867d_airplay_target exact;
static server_info_fn stock_server_info;
static setup_fn stock_setup;
static teardown_fn stock_teardown;
static cbc_init_fn stock_cbc_init;
static server_socket_fn server_socket_open;

static CFStringRef (*cf_string)(void *, const char *, uint32_t);
static void (*cf_release)(CFTypeRef);
static CFTypeRef (*cf_dict_get)(CFDictionaryRef, CFTypeRef);
static void (*cf_dict_set)(CFMutableDictionaryRef, CFTypeRef, CFTypeRef);
static int64_t (*cf_dict_get_i64)(CFDictionaryRef, CFTypeRef, OSStatus *);
static void (*cf_dict_set_i64)(CFMutableDictionaryRef, CFTypeRef, int64_t);
static void (*cf_dict_remove)(CFMutableDictionaryRef, CFTypeRef);
static CFIndex (*cf_dict_count)(CFDictionaryRef);
static void (*cf_dict_pairs)(CFDictionaryRef, const void **, const void **);
static CFMutableDictionaryRef (*cf_dict_new)(void *, CFIndex, const void *, const void *);
static CFIndex (*cf_array_count)(CFArrayRef);
static CFTypeRef (*cf_array_get)(CFArrayRef, CFIndex);
static void (*cf_array_append)(CFMutableArrayRef, CFTypeRef);
static CFMutableArrayRef (*cf_array_new)(void *, CFIndex, const void *);
static CFMutableArrayRef (*cf_array_copy)(void *, CFIndex, CFArrayRef);
static const void *dict_key_callbacks;
static const void *dict_value_callbacks;
static const void *array_callbacks;

static volatile int enabled;
static volatile int receiver_stop;
static int listen_fd = -1;
static int client_fd = -1;
static uint8_t master_key[16];
static int master_valid;
static uint64_t receive_ms;
static uint64_t receive_pts;

static CFStringRef string_new(const char *text)
{
    return cf_string ? cf_string(NULL, text, CF_UTF8) : NULL;
}

static CFTypeRef dict_get(CFDictionaryRef dictionary, const char *name)
{
    CFStringRef key = string_new(name);
    CFTypeRef value = key && dictionary ? cf_dict_get(dictionary, key) : NULL;
    if (key) cf_release(key);
    return value;
}

static void dict_set(CFMutableDictionaryRef dictionary, const char *name,
                     CFTypeRef value)
{
    CFStringRef key = string_new(name);
    if (key && dictionary && value) cf_dict_set(dictionary, key, value);
    if (key) cf_release(key);
}

static void dict_set_i64(CFMutableDictionaryRef dictionary, const char *name,
                         int64_t value)
{
    CFStringRef key = string_new(name);
    if (key && dictionary) cf_dict_set_i64(dictionary, key, value);
    if (key) cf_release(key);
}

static void dict_set_string(CFMutableDictionaryRef dictionary, const char *name,
                            const char *value)
{
    CFStringRef string = string_new(value);
    if (string) { dict_set(dictionary, name, string); cf_release(string); }
}

static void dict_remove(CFMutableDictionaryRef dictionary, const char *name)
{
    CFStringRef key = string_new(name);
    if (key && dictionary) cf_dict_remove(dictionary, key);
    if (key) cf_release(key);
}

static CFMutableDictionaryRef dict_clone(CFDictionaryRef source)
{
    const void *keys[64], *values[64];
    CFMutableDictionaryRef result;
    CFIndex count, i;
    if (!source) return NULL;
    count = cf_dict_count(source);
    if (count < 0 || count > 64) return NULL;
    result = cf_dict_new(NULL, 0, dict_key_callbacks, dict_value_callbacks);
    if (!result) return NULL;
    cf_dict_pairs(source, keys, values);
    for (i = 0; i < count; ++i) cf_dict_set(result, keys[i], values[i]);
    return result;
}

static int dict_i64(CFDictionaryRef dictionary, const char *name, int64_t *out)
{
    CFStringRef key = string_new(name);
    OSStatus error = -1;
    int64_t value = 0;
    if (key && dictionary) value = cf_dict_get_i64(dictionary, key, &error);
    if (key) cf_release(key);
    if (error) return -1;
    *out = value;
    return 0;
}

static void add_features(CFMutableDictionaryRef info)
{
    CFArrayRef old = (CFArrayRef)dict_get(info, "enabledFeatures");
    CFMutableArrayRef features = old ? cf_array_copy(NULL, 0, old) :
                                      cf_array_new(NULL, 0, array_callbacks);
    CFStringRef alt = string_new("altScreen");
    CFStringRef areas = string_new("viewAreas");
    if (features && alt) cf_array_append(features, alt);
    if (features && areas) cf_array_append(features, areas);
    if (features) dict_set(info, "enabledFeatures", features);
    if (areas) cf_release(areas);
    if (alt) cf_release(alt);
    if (features) cf_release(features);
}

static void add_display(CFMutableDictionaryRef info)
{
    CFArrayRef old = (CFArrayRef)dict_get(info, "displays");
    CFMutableArrayRef displays;
    CFMutableDictionaryRef display;
    if (!old || cf_array_count(old) < 1) return;
    displays = cf_array_copy(NULL, 0, old);
    display = dict_clone((CFDictionaryRef)cf_array_get(old, 0));
    if (!displays || !display) goto done;
    dict_remove(display, "primaryInputDevice");
    dict_remove(display, "edid");
    dict_remove(display, "platformLayer");
    dict_remove(display, "avcc");
    dict_remove(display, "xOffset");
    dict_remove(display, "yOffset");
    dict_remove(display, "windowWidth");
    dict_remove(display, "windowHeight");
    dict_set_i64(display, "type", STREAM_ALT);
    dict_set_i64(display, "maxFPS", 30);
    dict_set_i64(display, "features", 2);
    dict_set_string(display, "uuid", "b7e6c5a0-2222-4000-8000-000000000002");
    dict_set_string(display, "initialURL", "maps:/car/instrumentcluster/map");
    cf_array_append(displays, display);
    dict_set(info, "displays", displays);
done:
    if (display) cf_release(display);
    if (displays) cf_release(displays);
}

static CFDictionaryRef hooked_server_info(AirPlayReceiverSessionRef session,
                                           CFArrayRef properties, uint8_t *mac,
                                           OSStatus *error)
{
    CFDictionaryRef result = stock_server_info ?
        stock_server_info(session, properties, mac, error) : NULL;
    if (enabled && result) {
        add_features((CFMutableDictionaryRef)result);
        add_display((CFMutableDictionaryRef)result);
    }
    return result;
}

static CFDictionaryRef find_alt(CFDictionaryRef request)
{
    CFArrayRef streams = (CFArrayRef)dict_get(request, "streams");
    CFIndex i, count = streams ? cf_array_count(streams) : 0;
    for (i = 0; i < count; ++i) {
        CFDictionaryRef stream = (CFDictionaryRef)cf_array_get(streams, i);
        int64_t type = 0;
        if (!dict_i64(stream, "type", &type) && type == STREAM_ALT) return stream;
    }
    return NULL;
}

static void append_alt_response(CFMutableDictionaryRef response,
                                CFDictionaryRef request_stream, int port)
{
    CFArrayRef old = (CFArrayRef)dict_get(response, "streams");
    CFMutableArrayRef streams = old ? cf_array_copy(NULL, 0, old) :
                                     cf_array_new(NULL, 0, array_callbacks);
    CFMutableDictionaryRef stream = dict_clone(request_stream);
    if (streams && stream) {
        dict_set_i64(stream, "dataPort", port);
        dict_set_i64(stream, "streamID", STREAM_ALT);
        cf_array_append(streams, stream);
        dict_set(response, "streams", streams);
    }
    if (stream) cf_release(stream);
    if (streams) cf_release(streams);
}

static void *receiver_thread(void *unused)
{
    uint8_t input[16384];
    int fd;
    (void)unused;
    fd = accept(listen_fd, NULL, NULL);
    if (fd < 0) goto done;
    client_fd = fd;
    while (!receiver_stop) {
        ssize_t count = read(fd, input, sizeof(input));
        if (count <= 0) break;
        receive_ms += 5u;
        if (dcc_alt111_ingress_feed(input, (size_t)count,
                                    receive_ms, receive_pts)) break;
        receive_pts += 3000u;
    }
done:
    if (fd >= 0) close(fd);
    client_fd = -1;
    dcc_alt111_ingress_stop();
    return NULL;
}

static int start_receiver(uint64_t connection_id)
{
    int port = ALT_PORT;
    pthread_t thread;
    if (!master_valid || !server_socket_open) return -1;
    receiver_stop = 0;
    receive_ms = 0;
    receive_pts = 0;
    if (dcc_alt111_ingress_begin(master_key, connection_id)) return -1;
    if (server_socket_open(AF_INET_VALUE, SOCK_STREAM_VALUE, IPPROTO_TCP_VALUE,
                           -ALT_PORT, &port, -ALT_BUFFER, &listen_fd)) {
        dcc_alt111_ingress_stop();
        listen_fd = -1;
        return -1;
    }
    if (pthread_create(&thread, NULL, receiver_thread, NULL)) {
        close(listen_fd); listen_fd = -1; dcc_alt111_ingress_stop(); return -1;
    }
    pthread_detach(thread);
    return port;
}

static void stop_receiver(void)
{
    receiver_stop = 1;
    if (client_fd >= 0) { close(client_fd); client_fd = -1; }
    if (listen_fd >= 0) { close(listen_fd); listen_fd = -1; }
    delay(20);
    dcc_alt111_ingress_stop();
}

static OSStatus hooked_setup(AirPlayReceiverSessionRef session,
                             CFDictionaryRef request,
                             CFDictionaryRef *out_response)
{
    CFDictionaryRef response = NULL;
    CFDictionaryRef alt;
    OSStatus result;
    int64_t connection_id = 0;
    int port;
    if (!stock_setup) return -1;
    result = stock_setup(session, request, &response);
    if (result || !enabled || !response) goto done;
    add_features((CFMutableDictionaryRef)response);
    alt = find_alt(request);
    if (!alt || dict_i64(alt, "streamConnectionID", &connection_id) ||
        connection_id <= 0) goto done;
    stop_receiver();
    port = start_receiver((uint64_t)connection_id);
    if (port > 0) append_alt_response((CFMutableDictionaryRef)response, alt, port);
done:
    if (out_response) *out_response = response;
    else if (response) cf_release(response);
    return result;
}

static void hooked_teardown(AirPlayReceiverSessionRef session,
                            CFDictionaryRef request, OSStatus reason,
                            Boolean *out_done)
{
    if (enabled && (!request || find_alt(request))) stop_receiver();
    if (stock_teardown) stock_teardown(session, request, reason, out_done);
}

OSStatus AES_CBCFrame_Init(void *context, const uint8_t *key,
                           const uint8_t *iv, Boolean encrypt)
{
    void *return_address = __builtin_return_address(0);
    OSStatus result;
    if (!stock_cbc_init)
        stock_cbc_init = (cbc_init_fn)dlsym(RTLD_NEXT, "AES_CBCFrame_Init");
    if (!stock_cbc_init) return -1;
    result = stock_cbc_init(context, key, iv, encrypt);
    if (!result && !encrypt && key && exact.session_security_info &&
        (uintptr_t)return_address >= (uintptr_t)exact.session_security_info &&
        (uintptr_t)return_address < (uintptr_t)exact.session_security_info + 0x200u) {
        memcpy(master_key, key, sizeof(master_key));
        master_valid = 1;
    }
    return result;
}

static int resolve_cf(void)
{
#define RESOLVE(variable, name) do { variable = dlsym(RTLD_DEFAULT, name); if (!variable) return -1; } while (0)
    RESOLVE(cf_string, "CFStringCreateWithCString");
    RESOLVE(cf_release, "CFRelease");
    RESOLVE(cf_dict_get, "CFDictionaryGetValue");
    RESOLVE(cf_dict_set, "CFDictionarySetValue");
    RESOLVE(cf_dict_get_i64, "CFDictionaryGetInt64");
    RESOLVE(cf_dict_set_i64, "CFDictionarySetInt64");
    RESOLVE(cf_dict_remove, "CFDictionaryRemoveValue");
    RESOLVE(cf_dict_count, "CFDictionaryGetCount");
    RESOLVE(cf_dict_pairs, "CFDictionaryGetKeysAndValues");
    RESOLVE(cf_dict_new, "CFDictionaryCreateMutable");
    RESOLVE(cf_array_count, "CFArrayGetCount");
    RESOLVE(cf_array_get, "CFArrayGetValueAtIndex");
    RESOLVE(cf_array_append, "CFArrayAppendValue");
    RESOLVE(cf_array_new, "CFArrayCreateMutable");
    RESOLVE(cf_array_copy, "CFArrayCreateMutableCopy");
    RESOLVE(server_socket_open, "ServerSocketOpen");
    RESOLVE(dict_key_callbacks, "kCFLDictionaryKeyCallBacksCFLTypes");
    RESOLVE(dict_value_callbacks, "kCFLDictionaryValueCallBacksCFLTypes");
    RESOLVE(array_callbacks, "kCFLArrayCallBacksCFLTypes");
#undef RESOLVE
    return 0;
}

void dcc_airplay_hook_init(void)
{
    uint32_t copy_expected[2] = {0xe92d4ff0u, 0xe24dde4du};
    uint32_t setup_expected[2] = {0xe92d4ff0u, 0xe59f4f60u};
    uint32_t teardown_expected[2] = {0xe92d4ff0u, 0xe24dd04cu};
    uint32_t setup_literal;
    enabled = 0;
    if (dcc_p0867d_airplay_resolve(&exact) || resolve_cf()) return;
    setup_literal = *(const uint32_t *)(exact.base + 0x17548u);
    stock_server_info = (server_info_fn)dcc_runtime_hook_install(
        exact.copy_server_info, hooked_server_info, copy_expected,
        DCC_ARM32_COPY_TWO, 0);
    stock_setup = (setup_fn)dcc_runtime_hook_install(
        exact.setup, hooked_setup, setup_expected,
        DCC_ARM32_RELOCATE_LDR_R4_LITERAL, setup_literal);
    stock_teardown = (teardown_fn)dcc_runtime_hook_install(
        exact.teardown, hooked_teardown, teardown_expected,
        DCC_ARM32_COPY_TWO, 0);
    if (!stock_server_info || !stock_setup || !stock_teardown) return;
    enabled = 1;
}
