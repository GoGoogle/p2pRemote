#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h> // 新增: 界面控件支持
#include <stdio.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib") // 新增: 界面库

// --- 调试开关 ---
#define ENABLE_TRAY   1       // 1: 开启托盘右键退出 (调试用)

// --- 资源 ID ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define ID_TRAY_EXIT  2001

// --- 完整的 STUN 列表 (按要求配置) ---
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

// --- 协议定义 ---
#pragma pack(push, 1)
typedef struct {
    unsigned short type; unsigned short length; 
    unsigned int magic; unsigned char id[12];
} StunHeader;
#pragma pack(pop)

typedef struct {
    unsigned int magic;
    int type; // 1:移动, 2:点击
    int x, y;
} P2PPacket;

#define CLR_LAN       RGB(0, 255, 255) // 青色
#define CLR_WAN       RGB(0, 255, 0)   // 绿色
#define CLR_TRY       RGB(255, 255, 0) // 黄色

#endif
