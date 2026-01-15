#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_srv;
SOCKET g_sock;

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) return TRUE;
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDOK) {
            char buf[64];
            if (GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64) > 0) {
                g_srv.sin_family = AF_INET;
                g_srv.sin_port = htons(P2P_PORT);
                g_srv.sin_addr.s_addr = inet_addr(buf);
                if (g_srv.sin_addr.s_addr == INADDR_NONE) {
                    MessageBoxA(h, "请输入有效的 IP 地址！", "格式错误", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                EndDialog(h, IDOK);
            }
            return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) { EndDialog(h, IDCANCEL); return TRUE; }
    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    
    // 初始化通用控件（确保对话框渲染正常）
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);
    
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 弹出手动输入界面
    if (DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) return 0;

    g_clr = CLR_WAN; // 连接后点变为绿色指示

    while(1) {
        HDC hdc = GetDC(NULL); POINT p; GetCursorPos(&p);
        HBRUSH br = CreateSolidBrush(g_clr); 
        HGDIOBJ old = SelectObject(hdc, br);
        
        // 在鼠标下方绘制彩色小点指示状态
        Ellipse(hdc, p.x + 15, p.y + 15, p.x + 25, p.y + 25);
        
        SelectObject(hdc, old);
        DeleteObject(br); 
        ReleaseDC(NULL, hdc);

        // 如果按下左键，发送移动和点击指令
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
