#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

// --- 调试配置 ---
#define ENABLE_TRAY   1       // 1: 显示托盘图标, 0: 完全隐藏

// --- 资源 ID (务必与 rc 文件严格对应) ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define ID_TRAY_EXIT  2001

// --- 按照您的要求配置的 9 个 STUN 地址 ---
static const char* STUN_SERVERS[] = {
    "stun.qq.com:3478",
    "stun.aliyun.com:3478",
    "stun.huawei.com:3478",
    "stun.cloudflare.com:3478",
    "stun.l.google.com:19302",
    "stun1.l.google.com:19302",
    "stun2.l.google.com:19302",
    "stun3.l.google.com:19302",
    "stun4.l.google.com:19302"
};
#define STUN_COUNT 9

#define P2P_PORT      9000
#define LAN_PORT      8888
#define AUTH_MAGIC    0xABCDEF12

#pragma pack(push, 1)
typedef struct {
    unsigned short type;
    unsigned short length; 
    unsigned int magic;
    unsigned char id[12];
} StunHeader;
#pragma pack(pop)

typedef struct {
    unsigned int magic;
    int type; // 1:移动, 2:点击
    int x, y;
} P2PPacket;

#define CLR_LAN       RGB(0, 255, 255)
#define CLR_WAN       RGB(0, 255, 0)
#define CLR_TRY       RGB(255, 255, 0)

#endif
