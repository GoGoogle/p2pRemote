#include "p2p_config.h"

// GDI+ C 接口定义
typedef void GpBitmap;
typedef void GpImage;
typedef struct { UINT32 v; PVOID p1; BOOL b1, b2; } Gsi;
typedef struct { CLSID Clsid; GUID ListGuid; const WCHAR* MediaType; const WCHAR* FileNameExtension; const WCHAR* MimeType; const WCHAR* FormatDescription; const WCHAR* CodecName; const WCHAR* DllName; const WCHAR* FormatID; UINT Flags; UINT Version; UINT SigCount; UINT SigSize; const BYTE* SigPattern; const BYTE* SigMask; } GpCodec;

extern __declspec(dllimport) int WINAPI GdiplusStartup(ULONG_PTR*, Gsi*, void*);
extern __declspec(dllimport) int WINAPI GdipCreateBitmapFromHBITMAP(HBITMAP, HPALETTE, GpBitmap**);
extern __declspec(dllimport) int WINAPI GdipSaveImageToStream(GpImage*, IStream*, const CLSID*, const void*);
extern __declspec(dllimport) int WINAPI GdipDisposeImage(GpImage*);
extern __declspec(dllimport) int WINAPI GdipGetImageEncodersSize(UINT*, UINT*);
extern __declspec(dllimport) int WINAPI GdipGetImageEncoders(UINT, UINT, GpCodec*);

SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
NOTIFYICONDATAW g_nid = { 0 };
ULONG_PTR g_gdiToken;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GdipGetImageEncodersSize(&num, &size);
    GpCodec* pIci = (GpCodec*)malloc(size);
    GdipGetImageEncoders(num, size, pIci);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pIci[j].MimeType, format) == 0) {
            *pClsid = pIci[j].Clsid;
            free(pIci); return j;
        }
    }
    free(pIci); return -1;
}

void GetIPs(char* loc, char* pub) {
    strcpy(pub, "探测中...");
    char host[256]; gethostname(host, 256);
    struct hostent* he = gethostbyname(host);
    if (he) strcpy(loc, inet_ntoa(*(struct in_addr*)he->h_addr));

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int to = 800; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
    for (int i = 0; i < STUN_COUNT; i++) {
        char sh[64]; int sp; sscanf(STUN_SERVERS[i], "%[^:]:%d", sh, &sp);
        struct hostent* she = gethostbyname(sh);
        if (!she) continue;
        struct sockaddr_in sa = { AF_INET, htons(sp), *((struct in_addr*)she->h_addr) };
        unsigned char req[20] = {0}; *(unsigned short*)req = htons(0x0001); *(unsigned int*)(req+4) = htonl(0x2112A442);
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));
        unsigned char buf[512]; struct sockaddr_in f; int fl = sizeof(f);
        if (recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&f, &fl) > 20) {
            for (int j = 20; j < 500; j++) {
                if (buf[j] == 0x00 && buf[j+1] == 0x01) {
                    struct in_addr a; a.S_un.S_addr = *(unsigned int*)(buf + j + 8);
                    strcpy(pub, inet_ntoa(a)); closesocket(s); return;
                }
            }
        }
    }
    closesocket(s);
}

DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScr = GetDC(NULL);
    CLSID jpgClsid; GetEncoderClsid(L"image/jpeg", &jpgClsid);
    int fid = 0;
    while (1) {
        if (!g_hasClient) { Sleep(200); continue; }
        fid++;
        HDC hdcMem = CreateCompatibleDC(hdcScr);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
        SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GpBitmap* bmp = NULL; GdipCreateBitmapFromHBITMAP(hBmp, NULL, &bmp);
        IStream* stream = NULL; CreateStreamOnHGlobal(NULL, TRUE, &stream);
        GdipSaveImageToStream((GpImage*)bmp, stream, &jpgClsid, NULL);
        HGLOBAL hg = NULL; GetHGlobalFromStream(stream, &hg);
        unsigned char* d = (unsigned char*)GlobalLock(hg);
        int sz = (int)GlobalSize(hg);
        for (int i = 0; i < sz; i += CHUNK_SIZE) {
            P2PPacket p = { AUTH_MAGIC, 2, fid, i, sz };
            p.slice_size = (sz - i < CHUNK_SIZE) ? (sz - i) : CHUNK_SIZE;
            memcpy(p.data, d + i, p.slice_size);
            sendto(g_srvSock, (char*)&p, sizeof(p), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
        }
        GlobalUnlock(hg); stream->lpVtbl->Release(stream);
        GdipDisposeImage((GpImage*)bmp); DeleteObject(hBmp); DeleteDC(hdcMem);
        Sleep(50); 
    }
    return 0;
}

LRESULT CALLBACK SrvProc(HWND hw, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TRAY_MSG && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p); HMENU h = CreatePopupMenu();
        AppendMenuW(h, MF_STRING, ID_TRAY_EXIT, L"退出服务端");
        SetForegroundWindow(hw); TrackPopupMenu(h, TPM_RIGHTBUTTON, p.x, p.y, 0, hw, NULL);
        DestroyMenu(h);
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid); ExitProcess(0);
    }
    return DefWindowProc(hw, m, w, l);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);
    Gsi gsi = {1, NULL, FALSE, FALSE}; GdiplusStartup(&g_gdiToken, &gsi, NULL);
    char lip[64], pip[64]; GetIPs(lip, pip);
    wchar_t info[256]; swprintf(info, 256, L"服务端启动\n内网:%S\n公网:%S", lip, pip);
    MessageBoxW(NULL, info, L"Service", MB_OK);

    WNDCLASSW wc = {0}; wc.lpfnWndProc = SrvProc; wc.hInstance = hI; wc.lpszClassName = L"Srv";
    RegisterClassW(&wc); HWND hw = CreateWindowW(L"Srv", NULL, 0, 0, 0, 0, 0, 0, 0, hI, 0);
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hw; g_nid.uID = 1; g_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_MSG; g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Service"); Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_srvSock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srvSock, (struct sockaddr*)&ad, sizeof(ad));
    CreateThread(NULL, 0, ScreenThread, NULL, 0, NULL);

    P2PPacket p; struct sockaddr_in f; int fl = sizeof(f);
    while (1) {
        MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        fd_set rs; FD_ZERO(&rs); FD_SET(g_srvSock, &rs); struct timeval tv = {0, 10000};
        if (select(0, &rs, 0, 0, &tv) > 0) {
            if (recvfrom(g_srvSock, (char*)&p, sizeof(p), 0, (struct sockaddr*)&f, &fl) > 0) {
                if (p.magic == AUTH_MAGIC) {
                    g_clientAddr = f; g_hasClient = TRUE;
                    if (p.type == 1) { SetCursorPos(p.v1, p.v2); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
                    if (p.type == 3) { SetCursorPos(p.v1, p.v2); mouse_event(MOUSEEVENTF_RIGHTDOWN|MOUSEEVENTF_RIGHTUP,0,0,0,0); }
                    if (p.type == 5) { SetCursorPos(p.v1, p.v2); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
                    if (p.type == 4) { keybd_event((BYTE)p.v1, 0, p.v2, 0); }
                }
            }
        }
    }
    return 0;
}
