#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Mesh-rescue protocol, version 1.
 * Messages are plain text so they are easy to read in Wireshark.
 *
 * HELLO|<id hex>|<name>|<status>
 *
 * Step 2 will add:  MSG|<msg id hex>|<ttl>|<from id hex>|<text>
 */

#define MESH_PORT       47000
#define HELLO_INTERVAL  3000    /* ms between HELLO broadcasts */
#define PEER_TIMEOUT    10000   /* ms before a silent peer is removed */
#define NAME_LEN        32
#define MAX_PEERS       32
#define BUF_SIZE        256

enum {
    STATUS_OK = 0,
    STATUS_NEED_HELP = 1,
    STATUS_TRAPPED = 2
};

static const char *status_text(int s)
{
    switch (s) {
    case STATUS_OK:        return "OK";
    case STATUS_NEED_HELP: return "NEED HELP";
    case STATUS_TRAPPED:   return "TRAPPED";
    default:               return "?";
    }
}

#endif
