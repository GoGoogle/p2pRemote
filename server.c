#include "p2p_config.h"

// --- GDI+ 纯 C 接口声明 (手动声明以解决 cl.exe 编译报错) ---
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
    if (size == 0) return -1;
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

// 获取内网和公网 IP
void GetIPInfo(char* localIP, char* publicIP) {
    strcpy(publicIP, "未探测到");
    strcpy(localIP, "127.0.0.1");
    
    // 获取内网 IP
    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == 0) {
        struct hostent* pHost = gethostbyname(hostName);
        if (pHost) {
            struct in_addr addr;
            memcpy(&addr, pHost->h_addr_list[0], sizeof(struct in_addr));
            strcpy(localIP, inet_ntoa(addr));
        }
    }

    // STUN 探测公网 IP
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int timeout = 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    for (int i = 0; i < STUN_COUNT; i++) {
        char host[64]; int port;
        if (sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port) != 2) continue;
        struct hostent* he = gethostbyname(host);
        if (!he) continue;
        struct sockaddr_in sa = { AF_INET, htons(port), *((struct in_addr*)he->h_addr) };
        unsigned char req[20] = {0};
        *(unsigned short*)req = htons(0x0001);
        *(unsigned int*)(req + 4) = htonl(0x2112A442);
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));
        unsigned char buf[512];
        struct sockaddr_in from; int len = sizeof(from);
        if (recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&from, &len) > 20) {
            for (int j = 20; j < 500; j++) {
                if (buf[j] == 0x00 && buf[j+1] == 0x01) { // MAPPED-ADDRESS
                    unsigned int xip = *(unsigned int*)(buf + j + 8);
                    struct in_addr addr; addr.S_un.S_addr = xip;
                    strcpy(publicIP, inet_ntoa(addr));
                    closesocket(s); return;
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
    int frame_id = 0;
    while (1) {
        if (!g_hasClient) { Sleep(200); continue; }
        GpBitmap* bmp = NULL;
        HDC hdcMem = CreateCompatibleDC(hdcScr);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
        SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GdipCreateBitmapFromHBITMAP(hBmp, NULL, &bmp);
        IStream* stream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        GdipSaveImageToStream((GpImage*)bmp, stream, &jpgClsid, NULL);
        HGLOBAL hg = NULL; GetHGlobalFromStream(stream, &hg);
        unsigned char* data = (unsigned char*)GlobalLock(hg);
        int size = (int)GlobalSize(hg);
        frame_id++;
        for (int i = 0; i < size; i += CHUNK_SIZE) {
            P2PPacket pkt = { AUTH_MAGIC, 2, frame_id, i, size };
            pkt.slice_size = (size - i < CHUNK_SIZE) ? (size - i) : CHUNK_SIZE;
            memcpy(pkt.data, data + i, pkt.slice_size);
            sendto(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
        }
        GlobalUnlock(hg); stream->lpVtbl->Release(stream);
        GdipDisposeImage((GpImage*)bmp); DeleteObject(hBmp); DeleteDC(hdcMem);
        Sleep(40);
    }
    return 0;
}

LRESULT CALLBACK SrvWndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TRAY_MSG && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p); HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出服务端");
        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid); ExitProcess(0);
    }
    return DefWindowProc(hWnd, m, w, l);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);
    Gsi gsi = {1, NULL, FALSE, FALSE};
    GdiplusStartup(&g_gdiToken, &gsi, NULL);
    
    char localIP[64], publicIP[64];
    GetIPInfo(localIP, publicIP);
    wchar_t info[256];
    swprintf(info, 256, L"服务端启动成功\n内网: %S\n公网: %S", localIP, publicIP);
    MessageBoxW(NULL, info, L"Service", MB_OK);

    WNDCLASSW wc = {0}; wc.lpfnWndProc = SrvWndProc; wc.hInstance = hI; wc.lpszClassName = L"P2PSrvWnd";
    RegisterClassW(&wc); HWND hInv = CreateWindowW(L"P2PSrvWnd", NULL, 0, 0, 0, 0, 0, 0, 0, hI, 0);

    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hInv; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY_MSG;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Service 运行中");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_srvSock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srvSock, (struct sockaddr*)&ad, sizeof(ad));

    CreateThread(NULL, 0, ScreenThread, NULL, 0, NULL);
    P2PPacket pkt;
    struct sockaddr_in from; int fl = sizeof(from);
    while (1) {
        MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
        if (recvfrom(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&from, &fl) > 0) {
            if (pkt.magic == AUTH_MAGIC) {
                g_clientAddr = from; g_hasClient = TRUE;
                if (pkt.type == 1 || pkt.type == 3) {
                    SetCursorPos(pkt.offset, pkt.total_size);
                    DWORD flags = (pkt.type == 1) ? (MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP) : (MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP);
                    mouse_event(flags, 0, 0, 0, 0);
                }
            }
        }
    }
    return 0;
}
