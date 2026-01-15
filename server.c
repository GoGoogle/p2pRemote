#include "p2p_config.h"

// 托盘控制句柄
NOTIFYICONDATAW nid = { 0 };

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 1) {
        if (l == WM_RBUTTONUP) { // 右键菜单
            POINT p; GetCursorPos(&p);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出 Service");
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
        }
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
        exit(0);
    }
    return DefWindowProc(hWnd, m, w, l);
}

void SetupTray(HINSTANCE hI) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hI;
    wc.lpszClassName = L"TrayMsgHandler";
    RegisterClassW(&wc);
    HWND hWnd = CreateWindowW(L"TrayMsgHandler", NULL, 0, 0, 0, 0, 0, NULL, NULL, hI, NULL);

    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(nid.szTip, L"P2P Service 运行中");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

// 获取 IP 逻辑保持不变...
void GetLocalIPs(char* out) {
    char name[255];
    if (gethostname(name, sizeof(name)) == 0) {
        struct hostent* he = gethostbyname(name);
        if (he) {
            for (int i = 0; he->h_addr_list[i] != NULL; i++) {
                strcat(out, inet_ntoa(*(struct in_addr*)he->h_addr_list[i]));
                strcat(out, "  ");
            }
        }
    }
}

void GetStunIP(char* out) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int to = 1000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
    for(int i=0; i<STUN_COUNT; i++){
        char host[64]; int port; sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port);
        struct hostent* he = gethostbyname(host); if(!he) continue;
        struct sockaddr_in sa = { AF_INET, htons(port) };
        memcpy(&sa.sin_addr, he->h_addr, he->h_length);
        StunHeader req = { htons(0x0001), 0, htonl(0x2112A442) };
        sendto(s, (char*)&req, sizeof(req), 0, (struct sockaddr*)&sa, sizeof(sa));
        char buf[512]; struct sockaddr_in f; int fl = sizeof(f);
        int len = recvfrom(s, buf, 512, 0, (struct sockaddr*)&f, &fl);
        if(len > 20){
            unsigned char* p = (unsigned char*)buf + 20;
            while(p < (unsigned char*)buf + len){
                if(ntohs(*(unsigned short*)p) == 0x0020){
                    unsigned int xip = *(unsigned int*)(p+8) ^ htonl(0x2112A442);
                    struct in_addr in; in.S_un.S_addr = xip;
                    strcpy(out, inet_ntoa(in)); closesocket(s); return;
                }
                p += (4 + ntohs(*(unsigned short*)(p+2)));
            }
        }
    }
    strcpy(out, "探测失败"); closesocket(s);
}

DWORD WINAPI LanResp(LPVOID lp) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(LAN_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&a, sizeof(a));
    char b[64]; struct sockaddr_in c; int cl = sizeof(c);
    while(1) if(recvfrom(s, b, 64, 0, (struct sockaddr*)&c, &cl) > 0) sendto(s, "ACK", 3, 0, (struct sockaddr*)&c, cl);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    
    char local[128] = ""; GetLocalIPs(local);
    char pub[32] = ""; GetStunIP(pub);
    wchar_t info[512];
    swprintf(info, 512, L"服务端就绪\n局域网: %S\n公网: %S", local, pub);
    MessageBoxW(NULL, info, L"Service", MB_OK);

    if (ENABLE_TRAY) SetupTray(hI);
    CreateThread(NULL, 0, LanResp, NULL, 0, NULL);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&ad, sizeof(ad));

    P2PPacket pkt; struct sockaddr_in f; int fl = sizeof(f);
    MSG msg;
    while(1) {
        // 允许处理托盘消息
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);

        fd_set r; FD_ZERO(&r); FD_SET(s, &r);
        struct timeval tv = {0, 10000};
        if (select(0, &r, NULL, NULL, &tv) > 0) {
            if (recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl) > 0) {
                if (pkt.magic == AUTH_MAGIC) {
                    SetCursorPos(pkt.x, pkt.y);
                    if (pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }
            }
        }
    }
    return 0;
}
