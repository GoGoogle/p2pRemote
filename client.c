#include "p2p_config.h"

unsigned char* g_frameRecvBuf = NULL;
int g_lastFrameId = 0;
GpBitmap* g_pDisplayBitmap = NULL;
ULONG_PTR g_gdiplusToken;

DWORD WINAPI RecvThread(LPVOID lp) {
    g_frameRecvBuf = (unsigned char*)malloc(2 * 1024 * 1024); // 2MB 足够存 JPEG
    P2PPacket pkt;
    struct sockaddr_in f; int fl = sizeof(f);

    while(1) {
        int r = recvfrom(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl);
        if(r > 0 && pkt.magic == AUTH_MAGIC && pkt.type == 2) {
            // 如果收到的是新帧，清除旧缓冲（简单处理，不处理乱序重排）
            if (pkt.frame_id > g_lastFrameId) {
                // 收到一帧的最后一个包或新包开始
                if (pkt.offset + pkt.slice_size >= pkt.total_size) {
                    memcpy(g_frameRecvBuf + pkt.offset, pkt.data, pkt.slice_size);
                    
                    // 解码 JPEG
                    IStream* pStream = SHCreateMemStream(g_frameRecvBuf, pkt.total_size);
                    if (pStream) {
                        GpBitmap* pNewBmp = NULL;
                        if (GdipCreateBitmapFromStream(pStream, &pNewBmp) == 0) {
                            if (g_pDisplayBitmap) GdipDisposeImage((GpImage*)g_pDisplayBitmap);
                            g_pDisplayBitmap = pNewBmp;
                            InvalidateRect(g_mainWnd, NULL, FALSE);
                        }
                        pStream->lpVtbl->Release(pStream);
                    }
                    g_lastFrameId = pkt.frame_id;
                } else {
                    memcpy(g_frameRecvBuf + pkt.offset, pkt.data, pkt.slice_size);
                }
            }
        }
    }
    return 0;
}

LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        if (g_pDisplayBitmap) {
            GpGraphics* graphics = NULL;
            GdipCreateFromHDC(hdc, &graphics);
            RECT rc; GetClientRect(h, &rc);
            GdipDrawImageRectI(graphics, (GpImage*)g_pDisplayBitmap, 0, 0, rc.right, rc.bottom);
            GdipDeleteGraphics(graphics);
        }
        // 指示点
        HBRUSH br = CreateSolidBrush(CLR_ACTIVE); SelectObject(hdc, br);
        Ellipse(hdc, 10, 10, 22, 22); DeleteObject(br);
        EndPaint(h, &ps);
        return 0;
    }
    // 其余逻辑（托盘、退出、CMD 发送等）保持不变
    return DefWindowProc(h, m, w, l);
}
