#define NONAMELESSUNION
#include "p2p_config.h"
#include <gdiplus/gdiplusflat.h>

SOCKET g_sock;
struct sockaddr_in g_srv;
HWND g_mainWnd;
NOTIFYICONDATAW g_nid = {0};
unsigned char* g_recvBuf = NULL;
int g_lastId = 0;
GpBitmap* g_dispBmp = NULL;
ULONG_PTR g_gdiToken;

DWORD WINAPI RecvThread(LPVOID lp) {
    g_recvBuf = (unsigned char*)malloc(2048 * 1024);
    P2PPacket pkt;
    struct sockaddr_in f; int fl = sizeof(f);

    while(1) {
        int r = recvfrom(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl);
        if(r > 0 && pkt.magic == AUTH_MAGIC && pkt.type == 2) {
            if (pkt.frame_id >= g_lastId) {
                memcpy(g_recvBuf + pkt.offset, pkt.data, pkt.slice_size);
                if (pkt.offset + pkt.slice_size >= pkt.total_size) {
                    IStream* stream = SHCreateMemStream(g_recvBuf, pkt.total_size);
                    if (stream) {
                        GpBitmap* newBmp = NULL;
                        if (GdipCreateBitmapFromStream(stream, &newBmp) == 0) {
                            if (g_dispBmp) GdipDisposeImage((GpImage*)g_dispBmp);
                            g_dispBmp = newBmp;
                            InvalidateRect(g_mainWnd, NULL, FALSE);
                        }
                        stream->lpVtbl->Release(stream);
                    }
                    g_lastId = pkt.frame_id;
                }
            }
        }
    }
    return 0;
}

LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        if (g_dispBmp) {
            GpGraphics* g = NULL;
            GdipCreateFromHDC(hdc, &g);
            RECT rc; GetClientRect(h, &rc);
            GdipDrawImageRectI(g, (GpImage*)g_dispBmp, 0, 0, rc.right, rc.bottom);
            GdipDeleteGraphics(g);
        }
        HBRUSH br = CreateSolidBrush(CLR_ACTIVE); SelectObject(hdc, br);
        Ellipse(hdc, 10, 10, 25, 25); DeleteObject(br);
        EndPaint(h, &ps); return 0;
    }
    // ... 其他逻辑 ...
    return DefWindowProc(h, m, w, l);
}
