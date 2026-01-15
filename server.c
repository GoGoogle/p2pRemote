#include "p2p_config.h"

NOTIFYICONDATAW nid = { 0 };

LRESULT CALLBACK TrayProc(HWND hWnd, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_USER + 1 && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p);
        HMENU h = CreatePopupMenu();
        AppendMenuW(h, MF_STRING, ID_TRAY_EXIT, L"退出服务");
        SetForegroundWindow(hWnd);
        TrackPopupMenu(h, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        DestroyMenu(h);
    } else if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) {
        Shell_NotifyIconW(NIM_DELETE, &nid); exit(0);
    }
    return DefWindowProc(hWnd, m, w, l);
}

void GetIPs(char* loc, char* pub) {
    char name[255]; gethostname(name, 255);
    struct hostent* he = gethostbyname(name);
    if (he) for (int i=0; he->h_addr_list[i]; i++) { 
        strcat(loc, inet_ntoa(*(struct in_addr*)he->h_addr_list[i])); strcat(loc, " "); 
    }
    
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int to = 1000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
    for(int i=0; i<STUN_COUNT; i++) {
        char h[64]; int p; sscanf(STUN_SERVERS[i], "%[^:]:%d", h, &p);
        struct hostent* he2 = gethostbyname(h); if(!he2) continue;
        struct sockaddr_in sa = { AF_INET, htons(p) }; memcpy(&sa.sin_addr, he2->h_addr, he2->h_length);
        StunHeader req = { htons(0x0001), 0, htonl(0x2112A442) };
        sendto(s, (char*)&req, sizeof(req), 0, (struct sockaddr*)&sa, sizeof(sa));
        char buf[512]; struct sockaddr_in f; int fl = sizeof(f);
        if(recvfrom(s, buf, 512, 0, (struct sockaddr*)&f, &fl) > 20) {
            unsigned char* ptr = (unsigned char*)buf + 20;
            while(ptr < (unsigned char*)buf + 512) {
                if(ntohs(*(unsigned short*)ptr) == 0x0020) {
                    unsigned int xip = *(unsigned int*)(ptr+8) ^ htonl(0x2112A442);
                    struct in_addr in; in.S_un.S_addr = xip;
                    strcpy(pub, inet_ntoa(in)); closesocket(s); return;
                }
                ptr += (4 + ntohs(*(unsigned short*)(ptr+2)));
            }
        }
    }
    strcpy(pub, "探测失败"); closesocket(s);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    char lIP[128]="", pIP[32]=""; GetIPs(lIP, pIP);
    wchar_t info[512]; swprintf(info, 512, L"服务端启动\n局域网: %S\n公网: %S", lIP, pIP);
    MessageBoxW(NULL, info, L"Service Info", MB_OK);

    if (ENABLE_TRAY) {
        WNDCLASSW wc = {0}; wc.lpfnWndProc = TrayProc; wc.hInstance = hI; wc.lpszClassName = L"TrayV";
        RegisterClassW(&wc); HWND hw = CreateWindowW(L"TrayV", NULL, 0,0,0,0,0,0,0,hI,0);
        nid.cbSize = sizeof(nid); nid.hWnd = hw; nid.uID = 1; nid.uFlags = 6; nid.uCallbackMessage = WM_USER+1;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); wcscpy(nid.szTip, L"P2P Service");
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ad = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&ad, sizeof(ad));

    P2PPacket pkt; struct sockaddr_in f; int fl = sizeof(f); MSG msg;
    while(1) {
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
        fd_set r; FD_ZERO(&r); FD_SET(s, &r); struct timeval tv = {0, 10000};
        if(select(0, &r, 0, 0, &tv) > 0) {
            if(recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl) > 0) {
                SetCursorPos(pkt.x, pkt.y);
                if(pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0);
            }
        }
    }
    return 0;
}
