#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include "ui.h"
#include "protocol.h"
static Font font;
static Sound alarm;
static int audio_ok, muted, alert_active;
static double next_alarm;
static char alert_name[NAME_LEN];
static unsigned alert_id, alert_event;
static char reply_name[NAME_LEN];
static int awaiting_reply;
void ui_ack_target(unsigned *id, unsigned *event) { *id=alert_id; *event=alert_event; }
void ui_sos_started(void) { awaiting_reply=1; reply_name[0]=0; }
void ui_reply(const char *name) { snprintf(reply_name,sizeof reply_name,"%s",name); awaiting_reply=0; }
static struct { unsigned id, event; } seen[64];
static unsigned seen_count;
void ui_alert(unsigned id, unsigned event, const char *name) {
    for(unsigned i=0;i<seen_count && i<64;i++) if(seen[i].id==id && seen[i].event==event) return;
    unsigned slot=seen_count++%64; seen[slot].id=id; seen[slot].event=event;
    snprintf(alert_name,sizeof alert_name,"%s",name);
    alert_id=id; alert_event=event; alert_active=1; muted=0; next_alarm=0;
}
static int japanese, offset, cooldown_now;
static const char *labels[] = {"無事", "助けが必要", "閉じ込められた"};
static const char *english[] = {"OK", "NEED HELP", "TRAPPED"};
static Color colors[] = {{22,101,52,255},{146,64,14,255},{153,27,27,255}};
static Color bg[] = {{220,252,231,255},{254,243,199,255},{254,226,226,255}};
static const char *tr(const char *ja,const char *en) { return japanese ? ja : en; }
static void text(const char *s,int x,int y,int size,Color c) { DrawTextEx(font,s,(Vector2){x,y},size,1,c); }
static Rectangle button(int i) { return (Rectangle){24+i*252,120,240,48}; }
static Rectangle sos(void) { return (Rectangle){24,184,744,55}; }
void ui_open(void) {
    InitWindow(792,640,"mesh-rescue | Raylib");
    SetTargetFPS(60);
    InitAudioDevice();
    if(IsAudioDeviceReady()) {
        short samples[22050];
        for(int i=0;i<22050;i++) samples[i]=(short)(6000*sin(6.283185307*(i<11025?880:660)*i/22050.0));
        Wave w={22050,22050,16,1,samples};
        alarm=LoadSoundFromWave(w); audio_ok=alarm.frameCount>0;
    }
    font=GetFontDefault();
    const char *path="C:/Windows/Fonts/meiryo.ttc";
    if (FileExists(path)) {
        int n=0;
        int *cp=LoadCodepoints("無事助けが必要閉じ込められたあなた現在の状態近く人秒前まだいません送信済み再可能上下スクロール接続中に",&n);
        int glyphs[512], k=0;
        for(int i=32;i<127;i++) glyphs[k++]=i;
        for(int i=0;i<n;i++) { int seen=0; for(int j=0;j<k;j++) if(glyphs[j]==cp[i]) seen=1; if(!seen && k<512) glyphs[k++]=cp[i]; }
        UnloadCodepoints(cp);
        Font f=LoadFontEx(path,32,glyphs,k);
        if(f.texture.id && f.texture.id!=font.texture.id) {font=f;japanese=1;}
    }
}
int ui_should_close(void) { return WindowShouldClose() || IsKeyPressed(KEY_Q); }
int ui_action(void) {
    if(alert_active) {
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m=GetMousePosition();
            if(CheckCollisionPointRec(m,(Rectangle){130,380,250,55})) { alert_active=0; if(audio_ok) StopSound(alarm); return 4; }
            else if(CheckCollisionPointRec(m,(Rectangle){410,380,250,55})) { muted=1; if(audio_ok) StopSound(alarm); }
        }
        return -1;
    }
    if(IsKeyPressed(KEY_ONE)) return 0;
    if(IsKeyPressed(KEY_TWO)) return 1;
    if(IsKeyPressed(KEY_THREE)) return 2;
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m=GetMousePosition();
        for(int i=0;i<3;i++) if(CheckCollisionPointRec(m,button(i))) return i;
        if(!cooldown_now && CheckCollisionPointRec(m,sos())) return 3;
    }
    return -1;
}
void ui_draw(const char *name,int status,int port,const UiPeer *peers,int count,int cooldown) {
    cooldown_now=cooldown;
    offset-=(int)GetMouseWheelMove();
    if(IsKeyPressed(KEY_DOWN)) offset++;
    if(IsKeyPressed(KEY_UP)) offset--;
    if(offset>count-7) offset=count-7;
    if(offset<0) offset=0;
    BeginDrawing(); ClearBackground((Color){244,246,249,255});
    DrawRectangle(0,0,792,65,(Color){23,43,69,255});
    text("mesh-rescue",24,18,30,WHITE);
    text(TextFormat("%s: %s",tr("あなた","You"),name),24,82,22,DARKGRAY);
    for(int i=0;i<3;i++) { Rectangle r=button(i); DrawRectangleRec(r,bg[i]); if(i==status) DrawRectangleLinesEx(r,3,colors[i]); text(japanese?labels[i]:english[i],(int)r.x+12,(int)r.y+12,21,colors[i]); }
    DrawRectangleRec(sos(),cooldown?GRAY:RED);
    text(cooldown?TextFormat("SOS - %ds",cooldown):"SOS",345,197,30,WHITE);
    text(TextFormat("%s (%d)",tr("近くの人","Nearby people"),count),24,257,24,DARKGRAY);
    if(!count) text(tr("まだ近くに人がいません","Nobody nearby yet"),24,300,20,GRAY);
    for(int i=offset;i<count && i<offset+7;i++) {
        int y=294+(i-offset)*41, s=peers[i].status;
        DrawRectangle(24,y,744,37,WHITE);
        text(peers[i].name,34,y+7,20,DARKGRAY);
        text(TextFormat("%lus",peers[i].age),430,y+8,18,GRAY);
        if(s>=0 && s<=2) text(japanese?labels[s]:english[s],520,y+7,20,colors[s]);
    }
    if(reply_name[0]) text(TextFormat("%s has received your SOS.",reply_name),24,580,18,DARKGREEN);
    else if(awaiting_reply) text("SOS sent - waiting for acknowledgement",24,580,18,MAROON);
    text(port?TextFormat("UDP 47000 | http://localhost:%d",port):"UDP 47000 | Web unavailable",24,600,17,DARKGRAY);
    if(alert_active) {
        if(audio_ok && !muted && GetTime()>=next_alarm) { PlaySound(alarm); next_alarm=GetTime()+2; }
        DrawRectangle(0,0,792,640,(Color){0,0,0,180});
        DrawRectangle(90,125,612,350,RAYWHITE);
        DrawRectangle(90,125,612,75,MAROON);
        text("EMERGENCY SOS",210,145,32,WHITE);
        text(TextFormat("From: %s",alert_name),125,230,26,MAROON);
        text("Emergency assistance requested!",125,285,23,DARKGRAY);
        text(muted?"Alarm muted":audio_ok?"Emergency alarm active":"Audio unavailable",125,330,20,DARKGRAY);
        DrawRectangle(130,380,250,55,MAROON); text("ACKNOWLEDGE",150,395,23,WHITE);
        DrawRectangle(410,380,250,55,DARKGRAY); text("MUTE ALARM",445,395,23,WHITE);
    }
    EndDrawing();
}
void ui_close(void) { if(audio_ok) UnloadSound(alarm); if(IsAudioDeviceReady()) CloseAudioDevice(); if(japanese) UnloadFont(font); CloseWindow(); }
