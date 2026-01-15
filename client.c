#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_srv;
SOCKET g_sock;

// 自动扫描 (带短超时)
void DoScan() {
    int o = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&o, sizeof(o));
    struct sockaddr_in b = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&b, sizeof(b));
    
    struct timeval t = {0, 200000}; // 200ms 超时，防止界面卡顿
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    
    struct sockaddr_in f; int fl = sizeof(f); char buf[64];
    if(recvfrom(g_sock, buf, 64, 0, (struct sockaddr*)&f, &fl) > 0) {
        g_srv = f; g_srv.sin_port = htons(P2P_PORT); g_clr = CLR_LAN;
    }
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
        case WM_INITDIALOG:
            // 界面显示后再执行扫描，并把结果填入输入框
            DoScan(); 
            if (g_clr == CLR_LAN) {
                SetDlgItemTextA(h, IDC_EDIT_ID, inet_ntoa(g_srv.sin_addr));
                SetDlgItemTextA(h, 1002, "已自动发现局域网服务端！");
            }
            return TRUE;
        case WM_COMMAND:
            if(LOWORD(w) == IDOK) {
                char buf[64]; GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
                g_srv.sin_family = AF_INET; g_srv.sin_port = htons(P2P_PORT);
                g_srv.sin_addr.s_addr = inet_addr(buf);
                EndDialog(h, IDOK); return TRUE;
            }
            if(LOWORD(w) == IDCANCEL) { EndDialog(h, IDCANCEL); return TRUE; }
            break;
    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    
    // 1. 初始化通用控件 (关键修复)
    INITCOMMONCONTROLSEX icex; icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. 启动对话框 (带错误检查)
    INT_PTR ret = DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0);
    
    if (ret == -1) {
        char err[64]; sprintf(err, "界面加载失败! Error: %d", GetLastError());
        MessageBoxA(NULL, err, "Fatal Error", MB_OK|MB_ICONERROR);
        return 0;
    }
    if (ret != IDOK) return 0; // 用户点了退出

    // 3. 成功后进入控制循环
    if(g_clr != CLR_LAN) g_clr = CLR_WAN;

    while(1) {
        HDC hdc = GetDC(NULL); POINT p; GetCursorPos(&p);
        HBRUSH br = CreateSolidBrush(g_clr); SelectObject(hdc, br);
        Ellipse(hdc, p.x+15, p.y+15, p.x+25, p.y+25);
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
