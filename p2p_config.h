#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>

// --- 资源 ID 定义 (供 resource.rc 和 client.c 使用) ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001

// --- 网络配置 ---
static const char* STUN_SERVERS[] = {
    "stun.qq.com:3478", 
    "stun.aliyun.com:3478", 
    "stun.huawei.com:3478",
    "stun.cloudflare.com:3478", 
    "stun.l.google.com:19302"
};
#define STUN_COUNT 5
#define P2P_PORT   9000
#define LAN_PORT   8888
#define AUTH_MAGIC 0xABCDEF12

// --- 状态颜色 ---
#define CLR_LAN       RGB(0, 255, 255) // 青色：局域网
#define CLR_WAN       RGB(0, 255, 0)   // 绿色：公网直连
#define CLR_TRY       RGB(255, 255, 0) // 黄色：尝试中
#define CLR_ERR       RGB(255, 0, 0)   // 红色：离线

// --- 数据协议结构 ---
typedef struct {
    unsigned int magic;
    int type;      // 0:心跳, 1:移动, 2:点击, 3:剪贴板同步
    int x;
    int y;
    int data_len;  // 针对剪贴板文本长度
    char data[1024]; // 密信传输缓冲区
} P2PPacket;

// --- 安全加密逻辑 ---
static void SecureXOR(char* d, int l) {
    const char k[] = "Gemini_P2P_2026"; // 你的专属通信密匙
    if (l <= 0) return;
    for(int i=0; i<l; i++) d[i] ^= k[i % 15];
}

#endif
