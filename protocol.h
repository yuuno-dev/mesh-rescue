#ifndef PROTOCOL_H
#define PROTOCOL_H

/* mesh-rescue 通信ルール(バージョン1)
 * メッセージは普通のテキストなので、Wiresharkなどで読みやすい。
 *
 * HELLO|<ID(16進数)>|<名前>|<状態>
 *
 * ステップ2で追加する予定:
 *   MSG|<メッセージID(16進数)>|<TTL>|<送信者ID(16進数)>|<本文>
 */

#define MESH_PORT       47000   /* 端末どうしが使うUDPポート */
#define HELLO_INTERVAL  3000    /* HELLOを送る間隔(ミリ秒) */
#define PEER_TIMEOUT    10000   /* この時間聞こえない人は一覧から消す(ミリ秒) */
#define NAME_LEN        32      /* 名前の最大バイト数(終端文字を含む) */
#define MAX_PEERS       32      /* 一覧に入れられる人数 */
#define BUF_SIZE        256     /* 受信バッファの大きさ */

/* 状態の番号 */
enum {
    STATUS_OK = 0,
    STATUS_NEED_HELP = 1,
    STATUS_TRAPPED = 2
};

/* 状態の番号を表示用の文字に変える */
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