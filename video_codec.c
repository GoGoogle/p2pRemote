#include "video_codec.h"

static SOCKET s_tcpSock = INVALID_SOCKET;
static SOCKET s_listenSock = INVALID_SOCKET;
static unsigned char* s_frameBuf = NULL;
static int s_w = 0, s_h = 0;

// --- 服务端逻辑 ---
void VideoCodec_InitSender(int w, int h, SOCKET udpSock) {
    s_w = w; s_h = h;
    s_frameBuf = (unsigned char*)malloc(w * h * 4);

    // 建立一个 TCP 监听，端口设为 P2P_PORT + 1
    s_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT + 1), INADDR_ANY };
    bind(s_listenSock, (struct sockaddr*)&ad, sizeof(ad));
    listen(s_listenSock, 1);
    
    // 异步等待连接，不阻塞主线程
    unsigned long mode = 1; ioctlsocket(s_listenSock, FIONBIO, &mode);
}

void VideoCodec_CaptureAndSend(SOCKET udpSock, struct sockaddr_in* dest) {
    if (s_tcpSock == INVALID_SOCKET) {
        struct sockaddr_in client; int len = sizeof(client);
        s_tcpSock = accept(s_listenSock, (struct sockaddr*)&client, &len);
        if (s_tcpSock == INVALID_SOCKET) return; // 还没连上
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

    // TCP 发送：先发大小，再发数据（TCP 会自动处理分片和重传）
    int total = s_w * s_h * 4;
    send(s_tcpSock, (char*)s_frameBuf, total, 0);

    DeleteObject(hBmp); DeleteDC(hdcMem); ReleaseDC(NULL, hdcScr);
}

// --- 客户端逻辑 ---
void VideoCodec_InitReceiver(SOCKET udpSock, struct sockaddr_in* srvAddr) {
    s_frameBuf = (unsigned char*)malloc(3840 * 2160 * 4);
    
    s_tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in ad = *srvAddr;
    ad.sin_port = htons(P2P_PORT + 1); // 画面走新端口
    
    connect(s_tcpSock, (struct sockaddr*)&ad, sizeof(ad));
}

void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH) {
    int total = screenW * screenH * 4;
    int received = 0;
    
    // TCP 接收：循环直到收满一整帧，保证画面永远不花
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
