#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_srv;
SOCKET g_sock;

// 自动扫描逻辑
void ScanLAN(HWND hDlg) {
    int o = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&o, sizeof(o));
    struct sockaddr_in b = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&b, sizeof(b));
    
    struct timeval t = {0, 300000}; // 300ms 超时
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    
    struct sockaddr_in f; int fl = sizeof(f); char buf[64];
    if(recvfrom(g_sock, buf, 64, 0, (struct sockaddr*)&f, &fl) > 0) {
        g_srv = f; g_srv.sin_port = htons(P2P_PORT); g_clr = CLR_LAN;
        SetDlgItemTextA(hDlg, IDC_EDIT_ID, inet_ntoa(g_srv.sin_addr));
        SetDlgItemTextA(hDlg, IDC_STATUS, "状态: 已自动发现局域网设备");
    } else {
        SetDlgItemTextA(hDlg, IDC_STATUS, "状态: 未发现局域网，请手动输入公网 IP");
    }
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) {
        SetTimer(h, 100, 500, NULL); // 延迟半秒扫描，保证界面已画出
        return TRUE;
    }
    if (m == WM_TIMER) {
        KillTimer(h, 100);
        ScanLAN(h);
        return TRUE;
    }
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDOK) {
            char buf[64]; GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
            g_srv.sin_family = AF_INET; g_srv.sin_port = htons(P2P_PORT);
            g_srv.sin_addr.s_addr = inet_addr(buf);
            EndDialog(h, IDOK); return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) { EndDialog(h, IDCANCEL); return TRUE; }
    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);
    
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 重点：如果 DialogBox 返回 -1，弹出错误代码
    INT_PTR ret = DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0);
    if (ret == -1) {
        char err[64]; sprintf(err, "界面启动失败! 错误码: %d", GetLastError());
        MessageBoxA(NULL, err, "Error", MB_OK); return 0;
    }
    if (ret != IDOK) return 0;

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
