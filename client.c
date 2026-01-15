#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_srv;
SOCKET g_sock;

// 对话框处理程序
INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_INITDIALOG:
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(w) == IDOK) {
                char buf[64];
                GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
                g_srv.sin_family = AF_INET;
                g_srv.sin_port = htons(P2P_PORT);
                g_srv.sin_addr.s_addr = inet_addr(buf);
                EndDialog(h, IDOK);
                return TRUE;
            }
            if (LOWORD(w) == IDCANCEL) {
                EndDialog(h, IDCANCEL);
                PostQuitMessage(0);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

void AutoScan() {
    int o = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&o, sizeof(o));
    struct sockaddr_in b = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&b, sizeof(b));
    struct timeval t = {0, 300000}; setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    struct sockaddr_in f; int fl = sizeof(f); char buf[64];
    if(recvfrom(g_sock, buf, 64, 0, (struct sockaddr*)&f, &fl) > 0) {
        g_srv = f; g_srv.sin_port = htons(P2P_PORT); g_clr = CLR_LAN;
    }
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    AutoScan();

    // 重要修正：使用 GetModuleHandle(NULL) 确保资源从当前 EXE 加载
    INT_PTR ret = DialogBoxParamA(GetModuleHandle(NULL), MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0);
    
    if(ret != IDOK) return 0;
    if(g_clr != CLR_LAN) g_clr = CLR_WAN;

    while(1) {
        HDC hdc = GetDC(NULL); POINT p; GetCursorPos(&p);
        HBRUSH br = CreateSolidBrush(g_clr); SelectObject(hdc, br);
        Ellipse(hdc, p.x+15, p.y+15, p.x+25, p.y+25); // 彩色小点指示状态
        DeleteObject(br); ReleaseDC(NULL, hdc);

        if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            P2PPacket pkt = { AUTH_MAGIC, 2, p.x, p.y };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
            Sleep(50);
        }
        if(GetAsyncKeyState(VK_ESCAPE)) break;
        Sleep(20);
    }
    return 0;
}
