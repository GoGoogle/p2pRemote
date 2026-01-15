#include "p2p_config.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_target;
SOCKET g_sock;

// 对话框回调函数
INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDOK) {
            char buf[64];
            GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
            g_target.sin_family = AF_INET;
            g_target.sin_port = htons(P2P_PORT);
            g_target.sin_addr.s_addr = inet_addr(buf); // 这里可以改进为支持 ID 映射
            EndDialog(h, IDOK);
        } else if (LOWORD(w) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
        }
    }
    return 0;
}

// 绘制状态指示点
void DrawStatusDot() {
    HDC hdc = GetDC(NULL);
    POINT p; GetCursorPos(&p);
    HBRUSH brush = CreateSolidBrush(g_clr);
    SelectObject(hdc, brush);
    // 在光标右下方画一个小圆点
    Ellipse(hdc, p.x + 15, p.y + 15, p.x + 25, p.y + 25);
    DeleteObject(brush);
    ReleaseDC(NULL, hdc);
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);

    // 1. 弹出输入框
    if (DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) {
        return 0;
    }

    // 2. 初始化网络
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    g_clr = CLR_WAN; // 简单假设连接成功

    // 3. 进入控制循环
    while (1) {
        DrawStatusDot();
        
        // 左键点击同步
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT p; GetCursorPos(&p);
            P2PPacket pkt = { AUTH_MAGIC, 2, p.x, p.y };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_target, sizeof(g_target));
            Sleep(100); // 防抖
        }

        if (GetAsyncKeyState(VK_ESCAPE)) break; // ESC 键退出
        Sleep(20);
    }
    return 0;
}
