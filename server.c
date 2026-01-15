#include "p2p_config.h"

// --- 全局变量 ---
SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
NOTIFYICONDATAW g_nid = { 0 };

// --- 1. STUN 公网探测逻辑 (使用您指定的 9 个服务器) ---
void GetPublicIP(char* outIP) {
    strcpy(outIP, "探测中...");
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int timeout = 800; 
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    for (int i = 0; i < STUN_COUNT; i++) {
        char host[64]; int port;
        if (sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port) != 2) continue;

        struct hostent* he = gethostbyname(host);
        if (!he) continue;

        struct sockaddr_in sa = { AF_INET, htons(port) };
        memcpy(&sa.sin_addr, he->h_addr, he->h_length);

        // 构造 STUN Binding Request
        unsigned char req[20] = { 0 };
        *(unsigned short*)req = htons(0x0001); // Message Type
        *(unsigned int*)(req + 4) = htonl(0x2112A442); // Magic Cookie
        
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));

        unsigned char buf[512];
        struct sockaddr_in from; int len = sizeof(from);
        if (recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&from, &len) > 20) {
            // 解析 XOR-MAPPED-ADDRESS (简化的偏移量查找)
            for (int j = 20; j < 510; j++) {
                if (buf[j] == 0x00 && buf[j+1] == 0x20) { // XOR-MAPPED-ADDRESS Type
                    unsigned int xip = *(unsigned int*)(buf + j + 8) ^ htonl(0x2112A442);
                    struct in_addr addr; addr.S_un.S_addr = xip;
                    strcpy(outIP, inet_ntoa(addr));
                    closesocket(s); return;
                }
            }
        }
    }
    strcpy(outIP, "探测失败(请检查网络)");
    closesocket(s);
}

// --- 2. 屏幕抓取与分片发送线程 ---
DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    
    HDC hdcScr = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
    SelectObject(hdcMem, hBmp);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-Down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    int imgSize = w * h * 4;
    unsigned char* rawData = (unsigned char*)malloc(imgSize);

    while (1) {
        if (!g_hasClient) { Sleep(500); continue; }

        // 截图
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GetDIBits(hdcMem, hBmp, 0, h, rawData, &bmi, DIB_RGB_COLORS);

        // 分片发送 (切成 CHUNK_SIZE 的小包)
        for (int i = 0; i < imgSize; i += CHUNK_SIZE) {
            P2PPacket pkt;
            pkt.magic = AUTH_MAGIC;
            pkt.type = 2; // 屏幕数据
            pkt.x = i;    // 偏移量
            pkt.slice_size = (imgSize - i < CHUNK_SIZE) ? (imgSize - i) : CHUNK_SIZE;
            memcpy(pkt.data, rawData + i, pkt.slice_size);

            sendto(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
            
            // 网络风暴控制：每发 32 个包微休眠一下，防止 UDP 丢包太严重
            if ((i / CHUNK_SIZE) % 32 == 0) Sleep(1);
        }
        Sleep(50); // 控制帧率在大约 15-20 FPS
    }
    return 0;
}

// --- 3. 托盘消息处理 ---
LRESULT CALLBACK TrayProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
#if ENABLE_TRAY
    if (m == WM_TRAY_MSG && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出服务端 (Exit)");
        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        ExitProcess(0);
    }
#endif
    return DefWindowProc(hWnd, m, w, l);
}

// --- 4. 程序入口 ---
int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);

    // 获取并显示 IP 状态
    char locIP[128] = "", pubIP[64] = "";
    char name[255]; gethostname(name, 255);
    struct hostent* he = gethostbyname(name);
    if (he) {
        for(int i=0; he->h_addr_list[i]; i++) {
            strcat(locIP, inet_ntoa(*(struct in_addr*)he->h_addr_list[i]));
            strcat(locIP, " ");
        }
    }
    GetPublicIP(pubIP);

    wchar_t info[512];
    swprintf(info, 512, L"服务端已启动\n局域网IP: %S\n公网IP: %S\n\n请在控制端输入上述 IP 即可连接。", locIP, pubIP);
    MessageBoxW(NULL, info, L"P2P Service Info", MB_OK | MB_ICONINFORMATION);

    // 托盘初始化
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = TrayProc; wc.hInstance = hI; wc.lpszClassName = L"SrvTray";
    RegisterClassW(&wc); HWND hw = CreateWindowW(L"SrvTray", NULL, 0, 0, 0, 0, 0, 0, 0, hI, 0);

#if ENABLE_TRAY
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hw; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY_MSG;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Service 正在运行");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
#endif

    // 初始化网络
    g_srvSock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srvSock, (struct sockaddr*)&ad, sizeof(ad));

    // 开启屏幕发送线程
    CreateThread(NULL, 0, ScreenThread, NULL, 0, NULL);

    P2PPacket pkt;
    struct sockaddr_in from; int fl = sizeof(from);
    while (1) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);

        // 接收控制端指令
        fd_set r; FD_ZERO(&r); FD_SET(g_srvSock, &r);
        struct timeval tv = { 0, 10000 };
        if (select(0, &r, 0, 0, &tv) > 0) {
            if (recvfrom(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&from, &fl) > 0) {
                if (pkt.magic == AUTH_MAGIC) {
                    // 一旦收到控制端任何包，就开始向其发送屏幕
                    g_clientAddr = from;
                    g_hasClient = TRUE;

                    if (pkt.type == 1) { // 鼠标指令
                        SetCursorPos(pkt.x, pkt.y);
                        mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    }
                }
            }
        }
    }
    return 0;
}
