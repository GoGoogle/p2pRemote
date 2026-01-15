#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

// --- 资源 ID 定义 ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define IDC_STATUS    1002

// --- 绝对完整的 STUN 服务器列表 (含 Google 全家桶) ---
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

// --- 网络端口与魔数 ---
#define P2P_PORT      9000
#define LAN_PORT      8888
#define AUTH_MAGIC    0xABCDEF12

// --- STUN 报文头定义 (RFC 5389) ---
#pragma pack(push, 1)
typedef struct {
    unsigned short type;   // 0x0001: Binding Request
    unsigned short length; 
    unsigned int magic;    // 0x2112A442
    unsigned char id[12];
} StunHeader;
#pragma pack(pop)

// --- 核心协议封包结构 ---
typedef struct {
    unsigned int magic;
    int type; // 1:移动, 2:左键点击
    int x;
    int y;
    int data_len;
    char data[1024];
} P2PPacket;

// --- 指示颜色 ---
#define CLR_LAN       RGB(0, 255, 255) // 青色：局域网
#define CLR_WAN       RGB(0, 255, 0)   // 绿色：公网
#define CLR_TRY       RGB(255, 255, 0) // 黄色：握手
#define CLR_ERR       RGB(255, 0, 0)   // 红色：错误

#endif
