#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_srv;
SOCKET g_sock;
HWND g_hDlg = NULL;

// 扫描逻辑
void DoScan() {
    SetDlgItemTextA(g_hDlg, IDC_STATUS, "状态: 正在扫描局域网...");
    int o = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&o, sizeof(o));
    struct sockaddr_in b = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&b, sizeof(b));
    
    struct timeval t = {0, 300000}; // 300ms 超时
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    
    struct sockaddr_in f; int fl = sizeof(f); char buf[64];
    if(recvfrom(g_sock, buf, 64, 0, (struct sockaddr*)&f, &fl) > 0) {
        g_srv = f; g_srv.sin_port = htons(P2P_PORT); g_clr = CLR_LAN;
        SetDlgItemTextA(g_hDlg, IDC_EDIT_ID, inet_ntoa(g_srv.sin_addr));
        SetDlgItemTextA(g_hDlg, IDC_STATUS, "状态: 已发现局域网设备");
    } else {
        SetDlgItemTextA(g_hDlg, IDC_STATUS, "状态: 请手动输入 IP");
    }
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
        case WM_INITDIALOG:
            g_hDlg = h;
            // 延时一下再扫描，确保界面先画出来
            SetTimer(h, 1, 100, NULL);
            return TRUE;
        case WM_TIMER:
            KillTimer(h, 1);
            DoScan();
            return TRUE;
        case WM_COMMAND:
            if(LOWORD(w) == IDOK) {
                char buf[64]; GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
                if(strlen(buf) == 0) {
                    MessageBoxA(h, "请输入 IP 地址", "错误", MB_OK);
                    return TRUE;
                }
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
    
    // 1. 初始化控件库 (防止某些系统下不显示)
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. 尝试创建对话框
    // 使用 GetModuleHandle(NULL) 获取当前实例，最稳妥
    INT_PTR ret = DialogBoxParamA(GetModuleHandle(NULL), MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0);
    
    // 3. 错误诊断：如果 ret == -1，说明资源没加载成功
    if (ret == -1) {
        char err[128];
        sprintf(err, "致命错误：无法加载界面资源 (Error Code: %d)\n请检查 resource.rc 是否正确编译。", GetLastError());
        MessageBoxA(NULL, err, "启动失败", MB_OK | MB_ICONERROR);
        return 0;
    }
    
    if (ret != IDOK) return 0; // 用户点了取消或关闭

    // 4. 进入控制循环 (无黑框)
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
