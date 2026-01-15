#include "p2p_config.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

COLORREF g_dot_clr = CLR_TRY;
struct sockaddr_in g_srv_addr;
SOCKET g_sock;

// 对话框逻辑
INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if(m == WM_COMMAND && LOWORD(w) == IDOK) {
        char buf[64];
        GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
        g_srv_addr.sin_family = AF_INET;
        g_srv_addr.sin_port = htons(P2P_PORT);
        g_srv_addr.sin_addr.s_addr = inet_addr(buf);
        EndDialog(h, IDOK);
    } else if(m == WM_COMMAND && LOWORD(w) == IDCANCEL) EndDialog(h, IDCANCEL);
    return 0;
}

// 自动探测局域网
void ScanLAN() {
    int opt = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
    struct sockaddr_in b_addr = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&b_addr, sizeof(b_addr));
    
    struct timeval tv = {0, 500000};
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    struct sockaddr_in from; int flen = sizeof(from); char buf[64];
    if(recvfrom(g_sock, buf, 64, 0, (struct sockaddr*)&from, &flen) > 0) {
        g_srv_addr = from; g_srv_addr.sin_port = htons(P2P_PORT);
        g_dot_clr = CLR_LAN; // 发现局域网变青色
    }
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    ScanLAN(); // 先扫局域网

    if(DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) return 0;
    if(g_dot_clr != CLR_LAN) g_dot_clr = CLR_WAN; // 手动连接变绿色

    while(1) {
        // 绘制彩色小点
        HDC hdc = GetDC(NULL); POINT p; GetCursorPos(&p);
        HBRUSH br = CreateSolidBrush(g_dot_clr);
        SelectObject(hdc, br);
        Ellipse(hdc, p.x+15, p.y+15, p.x+25, p.y+25);
        DeleteObject(br); ReleaseDC(NULL, hdc);

        if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            P2PPacket pkt = { AUTH_MAGIC, 2, p.x, p.y };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv_addr, sizeof(g_srv_addr));
        }
        if(GetAsyncKeyState(VK_ESCAPE)) break;
        Sleep(20);
    }
    return 0;
}
