#include "video_codec.h"

static SOCKET s_tcpSock = INVALID_SOCKET;
static SOCKET s_listenSock = INVALID_SOCKET;
static unsigned char* s_frameBuf = NULL;
static int s_w = 0, s_h = 0;

// --- 服务端实现 ---
void VideoCodec_InitSender(int w, int h, SOCKET udpSock) {
    s_w = w; s_h = h;
    s_frameBuf = (unsigned char*)malloc(w * h * 4);

    s_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT + 1), INADDR_ANY };
    bind(s_listenSock, (struct sockaddr*)&ad, sizeof(ad));
    listen(s_listenSock, 1);
    
    unsigned long mode = 1; 
    ioctlsocket(s_listenSock, FIONBIO, &mode); // 非阻塞监听
}

void VideoCodec_CaptureAndSend(SOCKET udpSock, struct sockaddr_in* dest) {
    if (s_tcpSock == INVALID_SOCKET) {
        struct sockaddr_in client; int len = sizeof(client);
        s_tcpSock = accept(s_listenSock, (struct sockaddr*)&client, &len);
        if (s_tcpSock == INVALID_SOCKET) return; 
    }

    HDC hdcScr = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, s_w, s_h);
    SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, s_w, s_h, hdcScr, 0, 0, SRCCOPY);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = s_w; bmi.bmiHeader.biHeight = -s_h;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;

    GetDIBits(hdcMem, hBmp, 0, s_h, s_frameBuf, &bmi, DIB_RGB_COLORS);

    // 通过 TCP 发送整帧数据
    send(s_tcpSock, (char*)s_frameBuf, s_w * s_h * 4, 0);

    DeleteObject(hBmp); DeleteDC(hdcMem); ReleaseDC(NULL, hdcScr);
}

// --- 客户端实现 ---
void VideoCodec_InitReceiver(SOCKET udpSock, struct sockaddr_in* srvAddr) {
    s_frameBuf = (unsigned char*)malloc(3840 * 2160 * 4);
    s_tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    struct sockaddr_in tcpAddr = *srvAddr;
    tcpAddr.sin_port = htons(P2P_PORT + 1);
    
    // 尝试连接服务端的 TCP 图像端口
    connect(s_tcpSock, (struct sockaddr*)&tcpAddr, sizeof(tcpAddr));
}

void VideoCodec_HandlePacket(P2PPacket* pkt) {
    // 保持空实现，因为数据现在改走 TCP 了
}

void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH) {
    if (s_tcpSock == INVALID_SOCKET) return;

    int total = screenW * screenH * 4;
    int received = 0;
    // 循环接收直到一帧完整
    while (received < total) {
        int r = recv(s_tcpSock, (char*)s_frameBuf + received, total - received, 0);
        if (r <= 0) break;
        received += r;
    }

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW; bmi.bmiHeader.biHeight = -screenH;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;

    RECT rc; GetClientRect(hwnd, &rc);
    StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, screenW, screenH, s_frameBuf, &bmi, DIB_RGB_COLORS, SRCCOPY);
}
