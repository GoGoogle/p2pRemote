#include "video_codec.h"

static HDC s_hdcMem = NULL;
static HBITMAP s_hBmp = NULL;
static unsigned char* s_currFrame = NULL;
static unsigned char* s_lastFrame = NULL;
static int s_width = 0, s_height = 0;

void VideoCodec_InitSender(int w, int h) {
    s_width = w; s_height = h;
    HDC hdcScr = GetDC(NULL);
    s_hdcMem = CreateCompatibleDC(hdcScr);
    s_hBmp = CreateCompatibleBitmap(hdcScr, w, h);
    SelectObject(s_hdcMem, s_hBmp);
    int size = w * h * 4;
    s_currFrame = (unsigned char*)malloc(size);
    s_lastFrame = (unsigned char*)malloc(size);
    memset(s_lastFrame, 0, size);
    ReleaseDC(NULL, hdcScr);
}

void VideoCodec_CaptureAndSend(SOCKET s, struct sockaddr_in* dest) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = s_width; bmi.bmiHeader.biHeight = -s_height;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;

    HDC hdcScr = GetDC(NULL);
    BitBlt(s_hdcMem, 0, 0, s_width, s_height, hdcScr, 0, 0, SRCCOPY);
    GetDIBits(s_hdcMem, s_hBmp, 0, s_height, s_currFrame, &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdcScr);

    int imgSize = s_width * s_height * 4;
    // 增加：步进检查，提高对比效率
    for (int i = 0; i < imgSize; i += CHUNK_SIZE) {
        int currentChunkSize = (imgSize - i < CHUNK_SIZE) ? (imgSize - i) : CHUNK_SIZE;
        if (memcmp(s_currFrame + i, s_lastFrame + i, currentChunkSize) != 0) {
            memcpy(s_lastFrame + i, s_currFrame + i, currentChunkSize);
            P2PPacket pkt = { AUTH_MAGIC, 2, i, imgSize };
            pkt.slice_size = currentChunkSize;
            memcpy(pkt.data, s_currFrame + i, pkt.slice_size);
            sendto(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)dest, sizeof(*dest));
        }
    }
}

void VideoCodec_InitReceiver() {
    static unsigned char* buf = NULL;
    if (!buf) buf = (unsigned char*)malloc(3840 * 2160 * 4);
    s_currFrame = buf;
}

void VideoCodec_HandlePacket(P2PPacket* pkt) {
    if (pkt->x + pkt->slice_size <= 3840 * 2160 * 4) {
        memcpy(s_currFrame + pkt->x, pkt->data, pkt->slice_size);
    }
}

void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH) {
    if (!s_currFrame) return;
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW; bmi.bmiHeader.biHeight = -screenH;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
    RECT rc; GetClientRect(hwnd, &rc);
    StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, screenW, screenH, s_currFrame, &bmi, DIB_RGB_COLORS, SRCCOPY);
}
