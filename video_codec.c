#include "video_codec.h"

// 服务端私有
static HDC s_hdcMem = NULL;
static HBITMAP s_hBmp = NULL;
static unsigned char* s_rawData = NULL;
static int s_width = 0, s_height = 0;

// 客户端私有
static unsigned char* s_recvBuf = NULL;
static const int MAX_BUF_SIZE = 3840 * 2160 * 4;

// --- 服务端实现 ---
void VideoCodec_InitSender(int w, int h) {
    s_width = w; s_height = h;
    HDC hdcScr = GetDC(NULL);
    s_hdcMem = CreateCompatibleDC(hdcScr);
    s_hBmp = CreateCompatibleBitmap(hdcScr, w, h);
    SelectObject(s_hdcMem, s_hBmp);
    s_rawData = (unsigned char*)malloc(w * h * 4);
    ReleaseDC(NULL, hdcScr);
}

void VideoCodec_CaptureAndSend(SOCKET s, struct sockaddr_in* dest) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = s_width;
    bmi.bmiHeader.biHeight = -s_height; // 严格匹配 1.0.0 的 Top-Down 顺序
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    HDC hdcScr = GetDC(NULL);
    BitBlt(s_hdcMem, 0, 0, s_width, s_height, hdcScr, 0, 0, SRCCOPY);
    // 严格按照 1.0.0 的 GetDIBits 调用
    GetDIBits(s_hdcMem, s_hBmp, 0, s_height, s_rawData, &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdcScr);

    int imgSize = s_width * s_height * 4;
    for (int i = 0; i < imgSize; i += CHUNK_SIZE) {
        P2PPacket pkt = { AUTH_MAGIC, 2, i, imgSize };
        pkt.slice_size = (imgSize - i < CHUNK_SIZE) ? (imgSize - i) : CHUNK_SIZE;
        memcpy(pkt.data, s_rawData + i, pkt.slice_size);
        sendto(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)dest, sizeof(*dest));
    }
}

// --- 客户端实现 ---
void VideoCodec_InitReceiver() {
    if (!s_recvBuf) {
        s_recvBuf = (unsigned char*)malloc(MAX_BUF_SIZE);
        memset(s_recvBuf, 0, MAX_BUF_SIZE);
    }
}

void VideoCodec_HandlePacket(P2PPacket* pkt) {
    if (pkt->x + pkt->slice_size <= MAX_BUF_SIZE) {
        memcpy(s_recvBuf + pkt->x, pkt->data, pkt->slice_size);
    }
}

void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH) {
    if (!s_recvBuf) return;
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW;
    bmi.bmiHeader.biHeight = -screenH; // 严格匹配 1.0.0
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    RECT rc;
    GetClientRect(hwnd, &rc);
    
    // 严格按照 1.0.0 的 StretchDIBits 调用参数
    StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 
                  0, 0, screenW, screenH, 
                  s_recvBuf, &bmi, DIB_RGB_COLORS, SRCCOPY);
}
