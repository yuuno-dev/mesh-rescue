/* peer.c - ステップ1 + 近くの人の表示と状態ボタンがあるWebページ
 *
 * UDPブロードキャストを使い、サーバーなしで近くの端末を見つける。
 * 見つけた人の一覧は、ブラウザで開けるページにも表示する。
 *
 * ビルド: gcc peer.c -o peer.exe -lws2_32
 * 実行:   peer.exe Yuno   (名前を変えて2~3個起動する)
 * キー:   1 = OK   2 = 助けが必要   3 = 閉じ込められた   q = 終了
 * Web:    http://localhost:8080 を開く(ポート番号は画面に表示される)
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

#define WEB_PORT_FIRST 8080
#define WEB_PORT_LAST  8089

/* 近くにいる1人分の情報 */
typedef struct {
    int used;            /* 1 = このスロットを使っている */
    unsigned id;         /* 相手のID(起動ごとにランダム) */
    char name[NAME_LEN]; /* 相手の名前 */
    int status;          /* 相手の状態(OK / 助けが必要 / 閉じ込められた) */
    DWORD last_seen;     /* 最後に聞こえた時刻(ミリ秒) */
} Peer;

static Peer peers[MAX_PEERS];

/* 相手を一覧に追加する。すでに一覧にいる場合は情報を更新する。 */
static void update_peer(unsigned id, const char *name, int status)
{
    int i, slot = -1;
    for (i = 0; i < MAX_PEERS; i++) {
        if (peers[i].used && peers[i].id == id) { slot = i; break; }
        if (!peers[i].used && slot < 0) slot = i;
    }
    if (slot < 0) return; /* 一覧がいっぱいなので何もしない */
    peers[slot].used = 1;
    peers[slot].id = id;
    strncpy(peers[slot].name, name, NAME_LEN - 1);
    peers[slot].name[NAME_LEN - 1] = '\0';
    peers[slot].status = status;
    peers[slot].last_seen = GetTickCount();
}

/* PEER_TIMEOUT ミリ秒の間、聞こえなかった人を一覧から消す。 */
static void prune_peers(void)
{
    int i;
    DWORD now = GetTickCount();
    for (i = 0; i < MAX_PEERS; i++)
        if (peers[i].used && now - peers[i].last_seen > PEER_TIMEOUT)
            peers[i].used = 0;
}

/* 画面をクリアして、自分の状態と近くの人の一覧を表示する。 */
static void print_table(const char *my_name, int my_status, int web_port)
{
    int i, count = 0;
    DWORD now = GetTickCount();
    system("cls");
    printf("You: %s [%s]   (keys: 1=OK 2=NEED HELP 3=TRAPPED q=quit)\n",
           my_name, status_text(my_status));
    if (web_port > 0)
        printf("Page: http://localhost:%d\n", web_port);
    else
        printf("Page: not available (no free port)\n");
    printf("\nNearby people:\n");
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].used) continue;
        printf("  %-16s %-10s heard %lu s ago\n", peers[i].name,
               status_text(peers[i].status),
               (unsigned long)((now - peers[i].last_seen) / 1000));
        count++;
    }
    if (count == 0) printf("  (nobody yet)\n");
}

/* HELLOメッセージを1回送る。形式は HELLO|ID|名前|状態 。 */
static void send_hello(SOCKET s, struct sockaddr_in *to, unsigned id,
                       const char *name, int status)
{
    char buf[BUF_SIZE];
    int n = snprintf(buf, sizeof buf, "HELLO|%x|%s|%d", id, name, status);
    sendto(s, buf, n, 0, (struct sockaddr *)to, sizeof *to);
}

/* Webページ用のTCPソケットを開く。
 * 同じPCで複数起動してもぶつからないように、
 * 8080~8089の中から空いているポートを探す。 */
static SOCKET open_web(int *port_out)
{
    int port;
    *port_out = 0;
    for (port = WEB_PORT_FIRST; port <= WEB_PORT_LAST; port++) {
        struct sockaddr_in a;
        SOCKET w = socket(AF_INET, SOCK_STREAM, 0);
        if (w == INVALID_SOCKET) return INVALID_SOCKET;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons((unsigned short)port);
        if (bind(w, (struct sockaddr *)&a, sizeof a) == 0 &&
            listen(w, 5) == 0) {
            *port_out = port;
            return w;
        }
        closesocket(w);
    }
    return INVALID_SOCKET;
}

/* HTMLに入れても安全な文字列にする。
 * 名前はネットワークから届くので、< > & " を別の書き方に変える。 */
static void html_escape(char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    for (; *src && n + 7 < cap; src++) {
        const char *r = NULL;
        if (*src == '<') r = "&lt;";
        else if (*src == '>') r = "&gt;";
        else if (*src == '&') r = "&amp;";
        else if (*src == '"') r = "&quot;";
        if (r) {
            size_t l = strlen(r);
            memcpy(dst + n, r, l);
            n += l;
        } else {
            dst[n++] = *src;
        }
    }
    dst[n] = '\0';
}

/* ページ用のバッファに文字を追加する。バッファからあふれないようにする。 */
#define ADD(...) do { \
    if (n < cap) { \
        int w = snprintf(out + n, cap - n, __VA_ARGS__); \
        if (w > 0) n += (size_t)w; \
        if (n >= cap) n = cap - 1; \
    } \
} while (0)

/* HTMLページ全体を作り、その長さ(バイト数)を返す。 */
static int build_page(char *out, size_t cap, const char *my_name, int my_status)
{
    size_t n = 0;
    int i, count = 0;
    DWORD now = GetTickCount();
    char esc[NAME_LEN * 6 + 1];

    for (i = 0; i < MAX_PEERS; i++)
        if (peers[i].used) count++;

    html_escape(esc, sizeof esc, my_name);
    ADD("<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<meta http-equiv=\"refresh\" content=\"3\">"
        "<title>mesh-rescue</title><style>"
        "body{font-family:sans-serif;max-width:420px;margin:0 auto;padding:16px}"
        ".row{display:flex;justify-content:space-between;align-items:center;"
        "padding:10px 0;border-top:1px solid #ddd}"
        ".chip{padding:3px 10px;border-radius:8px;font-size:13px;font-weight:bold}"
        ".s0{background:#d9f2d9;color:#1b5e20}"
        ".s1{background:#fff0c2;color:#7a5200}"
        ".s2{background:#ffd6d6;color:#8a1111}"
        ".age{font-size:12px;color:#777}"
        ".btns{display:flex;gap:8px;margin:12px 0}"
        ".btn{flex:1;text-align:center;padding:12px 4px;border:1px solid #bbb;"
        "border-radius:8px;text-decoration:none;color:#222;font-size:15px}"
        ".btn.on{background:#e3eefc;border-color:#2a6fdb;font-weight:bold}"
        "</style></head><body>");
    ADD("<h1>mesh-rescue</h1>");
    ADD("<p>You are %s <span class=\"chip s%d\">%s</span></p>",
        esc, my_status, status_text(my_status));
    /* 状態ボタン。今の状態のボタンには on を付けて目立たせる。 */
    ADD("<div class=\"btns\">"
        "<a class=\"btn%s\" href=\"/status?s=0\">OK</a>"
        "<a class=\"btn%s\" href=\"/status?s=1\">Need help</a>"
        "<a class=\"btn%s\" href=\"/status?s=2\">Trapped</a></div>",
        my_status == STATUS_OK ? " on" : "",
        my_status == STATUS_NEED_HELP ? " on" : "",
        my_status == STATUS_TRAPPED ? " on" : "");
    ADD("<h2>Nearby people (%d)</h2>", count);
    if (count == 0)
        ADD("<p class=\"age\">Nobody nearby yet.</p>");
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].used) continue;
        html_escape(esc, sizeof esc, peers[i].name);
        ADD("<div class=\"row\"><div>%s<div class=\"age\">heard %lu s ago</div></div>"
            "<span class=\"chip s%d\">%s</span></div>",
            esc, (unsigned long)((now - peers[i].last_seen) / 1000),
            peers[i].status, status_text(peers[i].status));
    }
    ADD("</body></html>");
    return (int)n;
}

/* ブラウザからの要求1回に答える。
 * /status?s=N という要求なら、自分の状態をNに変えてトップページへ戻す。
 * それ以外はページを返す。
 * 状態を変えたときは1、変えなかったときは0を返す。 */
static int handle_web(SOCKET web, const char *my_name, int *my_status)
{
    SOCKET c = accept(web, NULL, NULL);
    char req[1024], head[256], body[12288];
    DWORD timeout_ms = 300;
    int n, s = 0, body_len, head_len, changed = 0;

    if (c == INVALID_SOCKET) return 0;
    /* ブラウザが接続だけして何も送ってこない場合に、ずっと待たないようにする。 */
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof timeout_ms);
    n = recv(c, req, sizeof req - 1, 0);
    if (n > 0) {
        req[n] = '\0';
        if (sscanf(req, "GET /status?s=%d", &s) == 1 && s >= 0 && s <= 2) {
            /* 状態ボタンが押された。状態を変えて、トップページへ戻す。 */
            *my_status = s;
            changed = 1;
            head_len = snprintf(head, sizeof head,
                "HTTP/1.1 303 See Other\r\n"
                "Location: /\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n");
            send(c, head, head_len, 0);
        } else {
            /* ふつうの要求。ページを返す。 */
            body_len = build_page(body, sizeof body, my_name, *my_status);
            head_len = snprintf(head, sizeof head,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n", body_len);
            send(c, head, head_len, 0);
            send(c, body, body_len, 0);
        }
    }
    closesocket(c);
    return changed;
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    SOCKET s, web;
    struct sockaddr_in local, bcast;
    char my_name[NAME_LEN];
    int my_status = STATUS_OK, yes = 1, web_port = 0;
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

    /* 同じPCで複数起動できるようにし、ブロードキャストも許可する。 */
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

    web = open_web(&web_port);

    for (;;) {
        fd_set rd;
        struct timeval tv = {0, 200000}; /* 最大200ミリ秒待つ */
        DWORD now;

        FD_ZERO(&rd);
        FD_SET(s, &rd);
        if (web != INVALID_SOCKET) FD_SET(web, &rd);
        if (select(0, &rd, NULL, NULL, &tv) > 0) {
            if (FD_ISSET(s, &rd)) {
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
            if (web != INVALID_SOCKET && FD_ISSET(web, &rd))
                if (handle_web(web, my_name, &my_status))
                    last_hello = 0; /* 状態が変わったので、すぐに送る */
        }

        while (_kbhit()) {
            int c = _getch();
            if (c == '1') my_status = STATUS_OK;
            else if (c == '2') my_status = STATUS_NEED_HELP;
            else if (c == '3') my_status = STATUS_TRAPPED;
            else if (c == 'q') {
                if (web != INVALID_SOCKET) closesocket(web);
                closesocket(s);
                WSACleanup();
                return 0;
            }
            last_hello = 0; /* 新しい状態をすぐに送る */
        }

        now = GetTickCount();
        if (now - last_hello >= HELLO_INTERVAL) {
            send_hello(s, &bcast, my_id, my_name, my_status);
            last_hello = now;
        }
        if (now - last_draw >= 1000) {
            prune_peers();
            print_table(my_name, my_status, web_port);
            last_draw = now;
        }
    }
}