#define NONAMELESSUNION
#include "p2p_config.h"
#include <gdiplus.h>

#ifdef __cplusplus
extern "C" {
#endif
    WINBASEAPI int WINAPI GdiplusStartup(ULONG_PTR*, const void*, void*);
    WINBASEAPI int WINAPI GdipCreateBitmapFromStream(IStream*, GpBitmap**);
    WINBASEAPI int WINAPI GdipCreateFromHDC(HDC, GpGraphics**);
    WINBASEAPI int WINAPI GdipDrawImageRectI(GpGraphics*, GpImage*, int, int, int, int);
    WINBASEAPI int WINAPI GdipDeleteGraphics(GpGraphics*);
    WINBASEAPI int WINAPI GdipDisposeImage(GpImage*);
#ifdef __cplusplus
}
#endif

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
            GpGraphics* g = NULL; GdipCreateFromHDC(hdc, &g);
            RECT rc; GetClientRect(h, &rc);
            GdipDrawImageRectI(g, (GpImage*)g_dispBmp, 0, 0, rc.right, rc.bottom);
            GdipDeleteGraphics(g);
        }
        HBRUSH br = CreateSolidBrush(CLR_ACTIVE); SelectObject(hdc, br);
        Ellipse(hdc, 10, 10, 22, 22); DeleteObject(br);
        EndPaint(h, &ps); return 0;
    }
    if (m == WM_LBUTTONDOWN) {
        RECT rc; GetClientRect(h, &rc);
        int rx = (LOWORD(l) * GetSystemMetrics(SM_CXSCREEN)) / rc.right;
        int ry = (HIWORD(l) * GetSystemMetrics(SM_CYSCREEN)) / rc.bottom;
        P2PPacket p = { AUTH_MAGIC, 1, 0, rx, ry };
        sendto(g_sock, (char*)&p, sizeof(p), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
    }
    if (m == WM_DESTROY) { PostQuitMessage(0); }
    return DefWindowProc(h, m, w, l);
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDOK) {
        char buf[64]; GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
        g_srv.sin_family = AF_INET; g_srv.sin_port = htons(P2P_PORT);
        g_srv.sin_addr.s_addr = inet_addr(buf);
        EndDialog(h, IDOK); return TRUE;
    }
    if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) ExitProcess(0);
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);
    struct { UINT32 v; PVOID p1; BOOL b1, b2; } gsi = {1, NULL, FALSE, FALSE};
    GdiplusStartup(&g_gdiToken, &gsi, NULL);
    if (DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) return 0;
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    WNDCLASSW wc = {0}; wc.lpfnWndProc = MainWndProc; wc.hInstance = hI; wc.lpszClassName = L"P2PView";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    g_mainWnd = CreateWindowW(L"P2PView", L"Control", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 854, 480, 0, 0, hI, 0);
    CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
