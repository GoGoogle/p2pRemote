#ifndef P2P_CONFIG_H
#define P2P_CONFIG_H

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

// --- 资源 ID 定义 (必须与 resource.rc 严格一致) ---
#define IDD_LOGIN     101
#define IDC_EDIT_ID   1001
#define IDC_STATUS    1002

// --- STUN 服务器列表 (穿透公网的关键) ---
static const char* STUN_SERVERS[] = {
    "stun.qq.com:3478",
    "stun.aliyun.com:3478",
    "stun.huawei.com:3478",
    "stun.cloudflare.com:3478",
    "stun.l.google.com:19302"
};
#define STUN_COUNT 5

// --- 网络端口与魔数 ---
#define P2P_PORT      9000    // 主通信端口
#define LAN_PORT      8888    // 局域网广播探测端口
#define AUTH_MAGIC    0xABCDEF12 // 握手校验魔数

// --- 状态指示颜色 ---
#define CLR_LAN       RGB(0, 255, 255) // 青色：局域网
#define CLR_WAN       RGB(0, 255, 0)   // 绿色：公网连接
#define CLR_TRY       RGB(255, 255, 0) // 黄色：尝试中/握手中
#define CLR_ERR       RGB(255, 0, 0)   // 红色：离线或连接失败

// --- 核心协议封包结构 ---
typedef struct {
    unsigned int magic;   // 必须为 AUTH_MAGIC
    int type;             // 0:心跳, 1:鼠标移动, 2:左键点击, 3:剪贴板数据
    int x;                // 屏幕 X 坐标
    int y;                // 屏幕 Y 坐标
    int data_len;         // data 字段的实际长度
    char data[1024];      // 密信/剪贴板/分片图像传输缓冲区
} P2PPacket;

// --- 密信安全：XOR 简易加解密 ---
static void SecureXOR(char* d, int l) {
    const char key[] = "Gemini_P2P_2026_Secure_Link";
    if (l <= 0) return;
    for(int i = 0; i < l; i++) {
        d[i] ^= key[i % (sizeof(key) - 1)];
    }
}

#endif // P2P_CONFIG_H
