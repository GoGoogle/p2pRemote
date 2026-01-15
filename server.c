#include "p2p_config.h"

SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
NOTIFYICONDATAW g_nid = { 0 };

void GetPublicIP(char* outIP) {
    strcpy(outIP, "探测失败");
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int timeout = 800; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    for (int i = 0; i < STUN_COUNT; i++) {
        char host[64]; int port; sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port);
        struct hostent* he = gethostbyname(host);
        if (!he) continue;
        struct sockaddr_in sa = { AF_INET, htons(port), *((struct in_addr*)he->h_addr) };
        unsigned char req[20] = { 0 }; *(unsigned short*)req = htons(0x0001); *(unsigned int*)(req + 4) = htonl(0x2112A442);
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));
        unsigned char buf[512]; struct sockaddr_in f; int fl = sizeof(f);
        if (recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&f, &fl) > 20) {
            for (int j = 20; j < 500; j++) {
                if (buf[j] == 0x00 && buf[j+1] == 0x01) { // MAPPED-ADDRESS
                    struct in_addr addr; addr.S_un.S_addr = *(unsigned int*)(buf + j + 12);
                    strcpy(outIP, inet_ntoa(addr)); closesocket(s); return;
                }
            }
        }
    }
    closesocket(s);
}

DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScr = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
    SelectObject(hdcMem, hBmp);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w; bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
    int imgSize = w * h * 4;
    unsigned char* rawData = (unsigned char*)malloc(imgSize);
    while (1) {
        if (!g_hasClient) { Sleep(500); continue; }
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GetDIBits(hdcMem, hBmp, 0, h, rawData, &bmi, DIB_RGB_COLORS);
        for (int i = 0; i < imgSize; i += CHUNK_SIZE) {
            P2PPacket pkt = { AUTH_MAGIC, 2, i, imgSize }; 
            pkt.slice_size = (imgSize - i < CHUNK_SIZE) ? (imgSize - i) : CHUNK_SIZE;
            memcpy(pkt.data, rawData + i, pkt.slice_size);
            sendto(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
        }
        Sleep(100); // 原始图传延迟无法避免，只能设高一点防止网络崩溃
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
    char locIP[64] = "Unknown", pubIP[64] = "";
    char name[256]; gethostname(name, 256);
    struct hostent* he = gethostbyname(name);
    if (he) strcpy(locIP, inet_ntoa(*(struct in_addr*)he->h_addr));
    GetPublicIP(pubIP);
    wchar_t info[256]; swprintf(info, 256, L"服务端启动\n内网:%S\n公网:%S", locIP, pubIP);
    MessageBoxW(NULL, info, L"Service", MB_OK);

    WNDCLASSW wc = {0}; wc.lpfnWndProc = SrvProc; wc.hInstance = hI; wc.lpszClassName = L"Srv";
    RegisterClassW(&wc); HWND hw = CreateWindowW(L"Srv", NULL, 0, 0, 0, 0, 0, 0, 0, hI, 0);
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hw; g_nid.uID = 1; g_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_MSG; g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Service"); Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_srvSock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srvSock, (struct sockaddr*)&ad, sizeof(ad));
    unsigned long mode = 1; ioctlsocket(g_srvSock, FIONBIO, &mode); // 非阻塞关键

    CreateThread(NULL, 0, ScreenThread, NULL, 0, NULL);
    P2PPacket pkt; struct sockaddr_in from; int fl = sizeof(from);
    while (1) {
        MSG msg; while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
        if (recvfrom(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&from, &fl) > 0) {
            if (pkt.magic == AUTH_MAGIC) {
                g_clientAddr = from; g_hasClient = TRUE;
                if (pkt.type == 1) { SetCursorPos(pkt.x, pkt.y); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
                if (pkt.type == 3) { SetCursorPos(pkt.x, pkt.y); mouse_event(MOUSEEVENTF_RIGHTDOWN|MOUSEEVENTF_RIGHTUP,0,0,0,0); }
                if (pkt.type == 5) { SetCursorPos(pkt.x, pkt.y); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0); }
                if (pkt.type == 4) { keybd_event((BYTE)pkt.x, 0, pkt.y, 0); } // 执行键盘
            }
        }
        Sleep(1);
    }
    return 0;
}
