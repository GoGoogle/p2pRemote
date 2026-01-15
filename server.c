#include "p2p_config.h"

// --- GDI+ 纯 C 接口手动声明 (防止 cl.exe 找不到定义) ---
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

int GetEncoder(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GdipGetImageEncodersSize(&num, &size);
    GpCodec* p = (GpCodec*)malloc(size);
    GdipGetImageEncoders(num, size, p);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(p[j].MimeType, format) == 0) {
            *pClsid = p[j].Clsid;
            free(p); return j;
        }
    }
    free(p); return -1;
}

void GetIPInfo(char* out) {
    char loc[64] = "127.0.0.1", pub[64] = "探测失败";
    char name[256]; gethostname(name, 256);
    struct hostent* he = gethostbyname(name);
    if (he) strcpy(loc, inet_ntoa(*(struct in_addr*)he->h_addr));
    
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int to = 800; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
    for (int i = 0; i < STUN_COUNT; i++) {
        char h[64]; int p; sscanf(STUN_SERVERS[i], "%[^:]:%d", h, &p);
        struct hostent* she = gethostbyname(h);
        if (!she) continue;
        struct sockaddr_in sa = { AF_INET, htons(p), *((struct in_addr*)she->h_addr) };
        unsigned char req[20] = {0}; *(unsigned short*)req = htons(0x0001); *(unsigned int*)(req+4) = htonl(0x2112A442);
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));
        unsigned char buf[512]; struct sockaddr_in f; int fl = sizeof(f);
        if (recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&f, &fl) > 20) {
            for (int j = 20; j < 500; j++) {
                if (buf[j] == 0x00 && buf[j+1] == 0x01) {
                    struct in_addr a; a.S_un.S_addr = *(unsigned int*)(buf + j + 8);
                    strcpy(pub, inet_ntoa(a)); break;
                }
            }
            if (strcmp(pub, "探测失败") != 0) break;
        }
    }
    closesocket(s);
    sprintf(out, "内网: %s\n公网: %s", loc, pub);
}

DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScr = GetDC(NULL);
    CLSID jpgClsid; GetEncoder(L"image/jpeg", &jpgClsid);
    int frame_id = 0;
    while (1) {
        if (!g_hasClient) { Sleep(200); continue; }
        HDC hdcMem = CreateCompatibleDC(hdcScr);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
        SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GpBitmap* bmp = NULL; GdipCreateBitmapFromHBITMAP(hBmp, NULL, &bmp);
        IStream* stream = NULL; CreateStreamOnHGlobal(NULL, TRUE, &stream);
        GdipSaveImageToStream((GpImage*)bmp, stream, &jpgClsid, NULL);
        
        HGLOBAL hg = NULL; GetHGlobalFromStream(stream, &hg);
        unsigned char* data = (unsigned char*)GlobalLock(hg);
        int total = (int)GlobalSize(hg);
        frame_id++;
        for (int i = 0; i < total; i += CHUNK_SIZE) {
            P2PPacket p = { AUTH_MAGIC, 2, frame_id, i, total };
            p.slice_size = (total - i < CHUNK_SIZE) ? (total - i) : CHUNK_SIZE;
            memcpy(p.data, data + i, p.slice_size);
            sendto(g_srvSock, (char*)&p, sizeof(p), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
        }
        GlobalUnlock(hg); stream->lpVtbl->Release(stream);
        GdipDisposeImage((GpImage*)bmp); DeleteObject(hBmp); DeleteDC(hdcMem);
        Sleep(40); 
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
    char info[256]; GetIPInfo(info); MessageBoxA(NULL, info, "Service", MB_OK);

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
        MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
        unsigned long mode = 1; ioctlsocket(g_srvSock, FIONBIO, &mode);
        if (recvfrom(g_srvSock, (char*)&p, sizeof(p), 0, (struct sockaddr*)&f, &fl) > 0) {
            if (p.magic == AUTH_MAGIC) {
                g_clientAddr = f; g_hasClient = TRUE;
                if (p.type == 1) { SetCursorPos(p.offset, p.total_size); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
                if (p.type == 3) { SetCursorPos(p.offset, p.total_size); mouse_event(MOUSEEVENTF_RIGHTDOWN|MOUSEEVENTF_RIGHTUP,0,0,0,0); }
                if (p.type == 4) { keybd_event((BYTE)p.offset, 0, p.total_size, 0); }
                if (p.type == 5) { SetCursorPos(p.offset, p.total_size); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
            }
        }
        Sleep(5);
    }
    return 0;
}
