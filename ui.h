#ifndef UI_H
#define UI_H
typedef struct { const char *name; int status; unsigned long age; } UiPeer;
void ui_open(void);
int ui_should_close(void);
int ui_action(void);
void ui_draw(const char *name, int status, int port, const UiPeer *peers, int count, int cooldown);
void ui_alert(unsigned id, unsigned event, const char *name);
void ui_ack_target(unsigned *id, unsigned *event);
void ui_reply(const char *name);
void ui_sos_started(void);
void ui_close(void);
#endif
