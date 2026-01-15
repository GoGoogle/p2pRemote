#include "video_codec.h"

// 服务端缓存
static unsigned char* s_currFrame = NULL;
static unsigned char* s_lastFrame = NULL;
static unsigned char* s_compBuf = NULL; // 压缩缓冲区
static int s_width = 0, s_height = 0;
static HDC s_hdcMem = NULL;
static HBITMAP s_hBmp = NULL;

// 客户端缓存
static unsigned char* s_recvBuf = NULL;
static const int MAX_BUF_SIZE = 3840 * 2160 * 4;

void VideoCodec_InitSender(int w, int h) {
    s_width = w; s_height = h;
    HDC hdcScr = GetDC(NULL);
    s_hdcMem = CreateCompatibleDC(hdcScr);
    s_hBmp = CreateCompatibleBitmap(hdcScr, w, h);
    SelectObject(s_hdcMem, s_hBmp);
    
    int size = w * h * 4;
    s_currFrame = (unsigned char*)malloc(size);
    s_lastFrame = (unsigned char*)malloc(size);
    s_compBuf = (unsigned char*)malloc(size + 1024);
    memset(s_lastFrame, 0, size);
    ReleaseDC(NULL, hdcScr);
}

// 极简压缩算法：只处理连续重复像素 (RLE 轻量版)
// 协议定义：pkt.type = 2 为原始/差异数据；新增 pkt.type = 6 为压缩数据
void VideoCodec_CaptureAndSend(SOCKET s, struct sockaddr_in* dest) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = s_width;
    bmi.bmiHeader.biHeight = -s_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    HDC hdcScr = GetDC(NULL);
    BitBlt(s_hdcMem, 0, 0, s_width, s_height, hdcScr, 0, 0, SRCCOPY);
    GetDIBits(s_hdcMem, s_hBmp, 0, s_height, s_currFrame, &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdcScr);

    int imgSize = s_width * s_height * 4;
    int rowSize = s_width * 4;

    for (int y = 0; y < s_height; y++) {
        int offset = y * rowSize;
        
        // 1. 行级对比：如果整行没变，直接跳过
        if (memcmp(s_currFrame + offset, s_lastFrame + offset, rowSize) == 0) {
            continue;
        }

        // 2. 更新缓存并发送变化的行
        memcpy(s_lastFrame + offset, s_currFrame + offset, rowSize);

        // 3. 将这一行切片发送
        for (int x = 0; x < rowSize; x += CHUNK_SIZE) {
            int sendSize = (rowSize - x < CHUNK_SIZE) ? (rowSize - x) : CHUNK_SIZE;
            P2PPacket pkt = { AUTH_MAGIC, 2, offset + x, imgSize };
            pkt.slice_size = sendSize;
            memcpy(pkt.data, s_currFrame + offset + x, sendSize);
            sendto(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)dest, sizeof(*dest));
        }
    }
}

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
    bmi.bmiHeader.biHeight = -screenH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    RECT rc; GetClientRect(hwnd, &rc);
    StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, screenW, screenH, s_recvBuf, &bmi, DIB_RGB_COLORS, SRCCOPY);
}
