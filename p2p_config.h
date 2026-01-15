#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

static const char* STUN_SERVERS[] = {
     "stun.aliyun.com:3478", "stun.qq.com:3478", "stun.huawei.com:3478", "stun.l.google.com:19302"
};
#define STUN_COUNT 4
#define P2P_PORT 9000
#define LAN_PORT 8888
#define AUTH_MAGIC 0xABCDEF12

// 状态色
#define CLR_LAN  RGB(0, 255, 255)
#define CLR_WAN  RGB(0, 255, 0)
#define CLR_TRY  RGB(255, 255, 0)
#define CLR_ERR  RGB(255, 0, 0)

typedef struct {
    unsigned int magic;
    int type; // 0:心跳, 1:移动, 2:点击, 3:剪贴板
    int x, y;
    int data_len;
    char data[1024];
} P2PPacket;

static void SecureXOR(char* d, int l) {
    const char k[] = "Gemini_P2P_2026";
    for(int i=0; i<l; i++) d[i] ^= k[i % 15];
}

#endif
