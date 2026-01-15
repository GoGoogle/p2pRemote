#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define ID_TRAY_EXIT  2001
#define WM_TRAY_MSG   (WM_USER + 2)

// STUN 列表 (已按要求更新)
static const char* STUN_SERVERS[] = {
    "stun.qq.com:3478", "stun.aliyun.com:3478", "stun.huawei.com:3478",
    "stun.cloudflare.com:3478", "stun.l.google.com:19302", "stun1.l.google.com:19302",
    "stun2.l.google.com:19302", "stun3.l.google.com:19302", "stun4.l.google.com:19302"
};

#define P2P_PORT      9000
#define AUTH_MAGIC    0xABCDEF12

// 数据包类型
#define PKT_CMD       1
#define PKT_SCREEN    2

#pragma pack(push, 1)
typedef struct {
    unsigned int magic;
    int type; // PKT_CMD
    int cmd;  // 1:移动+左击
    int x, y;
} CommandPacket;

typedef struct {
    unsigned int magic;
    int type; // PKT_SCREEN
    int offset;
    int size;
    unsigned char data[1024]; // 简单分片发送
} ScreenPacket;
#pragma pack(pop)

#endif
