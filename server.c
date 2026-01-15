#include "p2p_config.h"
#include "video_codec.h"

// --- 全局变量 ---
SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
NOTIFYICONDATAW g_nid = { 0 };

// --- 公网探测逻辑 (保持 1.0.0 结构并修正偏移) ---
void GetPublicIP(char* outIP) {
    strcpy(outIP, "探测失败");
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int timeout = 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    for (int i = 0; i < STUN_COUNT; i++) {
        char host[64]; int port;
        if (sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port) != 2) continue;
        struct hostent* he = gethostbyname(host);
        if (!he) continue;

        struct sockaddr_in sa = { AF_INET, htons(port) };
        memcpy(&sa.sin_addr, he->h_addr, he->h_length);

        unsigned char req[20] = { 0 };
        *(unsigned short*)req = htons(0x0001); 
        *(unsigned int*)(req + 4) = htonl(0x2112A442);
        
        sendto(s, (char*)req, 20, 0, (struct sockaddr*)&sa, sizeof(sa));

        unsigned char buf[512];
        struct sockaddr_in f; int len = sizeof(f);
        int r = recvfrom(s, (char*)buf, 512, 0, (struct sockaddr*)&f, &len);
        if (r > 20) {
            for (int j = 20; j < r - 8; j++) {
                if (buf[j] == 0x00 && (buf[j+1] == 0x01 || buf[j+1] == 0x20)) {
                    unsigned short family = ntohs(*(unsigned short*)(buf + j + 4));
                    if (family == 0x0001) {
                        struct in_addr addr;
                        if (buf[j+1] == 0x20) // XOR-MAPPED
                            addr.S_un.S_addr = *(unsigned int*)(buf + j + 8) ^ htonl(0x2112A442);
                        else // MAPPED
                            addr.S_un.S_addr = *(unsigned int*)(buf + j + 8);
                        strcpy(outIP, inet_ntoa(addr));
                        closesocket(s); return;
                    }
                }
            }
        }
    }
    closesocket(s);
}

// --- 屏幕抓取线程 (现在只负责循环调度) ---
DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    
    // 初始化公共库中的抓屏环境
    VideoCodec_InitSender(w, h);

    while (1) {
        if (!g_hasClient) { Sleep(500); continue; }

        // 调用公共函数进行抓取并发送
        VideoCodec_CaptureAndSend(g_srvSock, &g_clientAddr);

        // 控制逻辑循环频率
        Sleep(60); 
    }
    return 0;
}

LRESULT CALLBACK TrayProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TRAY_MSG && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出服务端");
        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        ExitProcess(0);
    }
    return DefWindowProc(hWnd, m, w, l);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);

    char locIP[128] = "", pubIP[64] = "";
    char name[255]; gethostname(name, 255);
    struct hostent* he = gethostbyname(name);
    if (he) strcpy(locIP, inet_ntoa(*(struct in_addr*)he->h_addr));
    GetPublicIP(pubIP);

    wchar_t info[512];
    swprintf(info, 512, L"服务端启动\n局域网: %S\n公网: %S", locIP, pubIP);
    MessageBoxW(NULL, info, L"Service Info", MB_OK);

    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = TrayProc; wc.hInstance = hI; wc.lpszClassName = L"SrvTray";
    RegisterClassW(&wc); HWND hw = CreateWindowW(L"SrvTray", NULL, 0, 0, 0, 0, 0, 0, 0, hI, 0);

    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hw; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY_MSG;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_srvSock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srvSock, (struct sockaddr*)&ad, sizeof(ad));

    // 设置为非阻塞模式，确保托盘菜单能够实时响应
    unsigned long mode = 1; ioctlsocket(g_srvSock, FIONBIO, &mode);

    CreateThread(NULL, 0, ScreenThread, NULL, 0, NULL);

    P2PPacket pkt; struct sockaddr_in from; int fl = sizeof(from);
    while (1) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);

        if (recvfrom(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&from, &fl) > 0) {
            if (pkt.magic == AUTH_MAGIC) {
                g_clientAddr = from; g_hasClient = TRUE;

                if (pkt.type == 1) { // 左键
                    SetCursorPos(pkt.x, pkt.y);
                    mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                } else if (pkt.type == 3) { // 右键
                    SetCursorPos(pkt.x, pkt.y);
                    mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
                } else if (pkt.type == 5) { // 双击
                    SetCursorPos(pkt.x, pkt.y);
                    mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                } else if (pkt.type == 4) { // 键盘
                    keybd_event((BYTE)pkt.x, 0, pkt.y, 0);
                }
            }
        }
        Sleep(1);
    }
    return 0;
}
