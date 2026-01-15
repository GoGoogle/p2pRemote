#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <ws2tcpip.h>
#include <shlwapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

// --- 全局配置 ---
#define ENABLE_TRAY   1

// --- 资源 ID ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define ID_TRAY_EXIT  2001
#define WM_TRAY_MSG   (WM_USER + 2)

// --- STUN 配置 (2026-01-15) ---
static const char* STUN_SERVERS[] = {
    "stun.qq.com:3478", "stun.aliyun.com:3478", "stun.huawei.com:3478",
    "stun.cloudflare.com:3478", "stun.l.google.com:19302", "stun1.l.google.com:19302",
    "stun2.l.google.com:19302", "stun3.l.google.com:19302", "stun4.l.google.com:19302"
};
#define STUN_COUNT 9

#define P2P_PORT      9000
#define AUTH_MAGIC    0xABCDEF12
#define CHUNK_SIZE    1200 

#pragma pack(push, 1)
typedef struct {
    unsigned int magic;
    int type;       // 1: CMD, 2: SCREEN_JPEG
    int frame_id;   // 帧序号
    int offset;     // 字节偏移
    int total_size; // JPEG 总大小
    int slice_size; // 本包大小
    unsigned char data[CHUNK_SIZE];
} P2PPacket;
#pragma pack(pop)

#define CLR_ACTIVE    RGB(0, 255, 0)

#endif
