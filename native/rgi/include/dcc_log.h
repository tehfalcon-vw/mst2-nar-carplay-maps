#ifndef DCC_LOG_H
#define DCC_LOG_H
#include "dcc_rgi.h"

typedef void (*DccLogCallback)(void *context,const DccMessage *message);
typedef struct {
    uint8_t frame[DCC_MAX_FRAME];
    size_t used;
    unsigned marker;
    int high_nibble;
    int capturing;
} DccLogReader;

void dcc_log_init(DccLogReader *reader);
/* Consume arbitrary splits of the stock mm-ipod diagnostic stream. Only bytes
 * inside CTRL[...] are decoded. A callback borrows reader storage until the
 * next feed call. Malformed captures reset atomically and report an error. */
DccResult dcc_log_feed(DccLogReader *reader,const char *text,size_t bytes,
    DccLogCallback callback,void *context,size_t *messages);

#endif
