/* peer.c - Step 1: find nearby peers with UDP broadcast (no server).
 *
 * Build:  gcc peer.c -o peer.exe -lws2_32
 * Run:    peer.exe Yuno        (run 2-3 copies with different names)
 * Keys:   1 = OK   2 = NEED HELP   3 = TRAPPED   q = quit
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

typedef struct {
    int used;
    unsigned id;
    char name[NAME_LEN];
    int status;
    DWORD last_seen;
} Peer;

static Peer peers[MAX_PEERS];

/* Add a new peer or refresh an existing one. */
static void update_peer(unsigned id, const char *name, int status)
{
    int i, slot = -1;
    for (i = 0; i < MAX_PEERS; i++) {
        if (peers[i].used && peers[i].id == id) { slot = i; break; }
        if (!peers[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return; /* table full */
    peers[slot].used = 1;
    peers[slot].id = id;
    strncpy(peers[slot].name, name, NAME_LEN - 1);
    peers[slot].name[NAME_LEN - 1] = '\0';
    peers[slot].status = status;
    peers[slot].last_seen = GetTickCount();
}

/* Forget peers we have not heard from for PEER_TIMEOUT ms. */
static void prune_peers(void)
{
    int i;
    DWORD now = GetTickCount();
    for (i = 0; i < MAX_PEERS; i++)
        if (peers[i].used && now - peers[i].last_seen > PEER_TIMEOUT)
            peers[i].used = 0;
}

static void print_table(const char *my_name, int my_status)
{
    int i, count = 0;
    DWORD now = GetTickCount();
    system("cls");
    printf("You: %s [%s]   (keys: 1=OK 2=NEED HELP 3=TRAPPED q=quit)\n\n",
           my_name, status_text(my_status));
    printf("Nearby people:\n");
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].used) continue;
        printf("  %-16s %-10s heard %lu s ago\n", peers[i].name,
               status_text(peers[i].status),
               (unsigned long)((now - peers[i].last_seen) / 1000));
        count++;
    }
    if (count == 0) printf("  (nobody yet)\n");
}

static void send_hello(SOCKET s, struct sockaddr_in *to, unsigned id,
                       const char *name, int status)
{
    char buf[BUF_SIZE];
    int n = snprintf(buf, sizeof buf, "HELLO|%x|%s|%d", id, name, status);
    sendto(s, buf, n, 0, (struct sockaddr *)to, sizeof *to);
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in local, bcast;
    char my_name[NAME_LEN];
    int my_status = STATUS_OK, yes = 1;
    unsigned my_id;
    DWORD last_hello = 0, last_draw = 0;

    strncpy(my_name, argc > 1 ? argv[1] : "anon", NAME_LEN - 1);
    my_name[NAME_LEN - 1] = '\0';
    my_id = (unsigned)(GetTickCount() ^ (GetCurrentProcessId() << 16)) | 1u;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) { printf("socket failed\n"); return 1; }

    /* Allow several copies on one PC, and allow broadcasting. */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof yes);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof yes);

    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(MESH_PORT);
    if (bind(s, (struct sockaddr *)&local, sizeof local) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        return 1;
    }

    memset(&bcast, 0, sizeof bcast);
    bcast.sin_family = AF_INET;
    bcast.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    bcast.sin_port = htons(MESH_PORT);

    for (;;) {
        fd_set rd;
        struct timeval tv = {0, 200000}; /* wait up to 200 ms */
        DWORD now;

        FD_ZERO(&rd);
        FD_SET(s, &rd);
        if (select(0, &rd, NULL, NULL, &tv) > 0) {
            char buf[BUF_SIZE];
            int n = recvfrom(s, buf, sizeof buf - 1, 0, NULL, NULL);
            if (n > 0) {
                unsigned id;
                char name[NAME_LEN];
                int status;
                buf[n] = '\0';
                if (sscanf(buf, "HELLO|%x|%31[^|]|%d", &id, name, &status) == 3
                    && id != my_id)
                    update_peer(id, name, status);
            }
        }

        while (_kbhit()) {
            int c = _getch();
            if (c == '1') my_status = STATUS_OK;
            else if (c == '2') my_status = STATUS_NEED_HELP;
            else if (c == '3') my_status = STATUS_TRAPPED;
            else if (c == 'q') { closesocket(s); WSACleanup(); return 0; }
            last_hello = 0; /* send the new status right away */
        }

        now = GetTickCount();
        if (now - last_hello >= HELLO_INTERVAL) {
            send_hello(s, &bcast, my_id, my_name, my_status);
            last_hello = now;
        }
        if (now - last_draw >= 1000) {
            prune_peers();
            print_table(my_name, my_status);
            last_draw = now;
        }
    }
}
