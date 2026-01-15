#include "p2p_config.h"

// --- 全局变量 ---
SOCKET g_sock;
struct sockaddr_in g_srv;
HWND g_mainWnd;
NOTIFYICONDATAW g_nid = {0};
unsigned char* g_screenBuf = NULL;
int g_scrW = 1920, g_scrH = 1080; // 这里的默认值应与服务端匹配，接收到首包后可动态调整

// --- 屏幕分片重组线程 ---
DWORD WINAPI RecvThread(LPVOID lp) {
    // 预分配缓冲区 (假设最大 4K 分辨率)
    g_screenBuf = (unsigned char*)malloc(3840 * 2160 * 4);
    P2PPacket pkt;
    struct sockaddr_in f; 
    int fl = sizeof(f);
    
    while(1) {
        int r = recvfrom(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl);
        if(r > 0 && pkt.magic == AUTH_MAGIC) {
            if(pkt.type == 2) { // 收到屏幕分片
                // pkt.x 是 offset, pkt.slice_size 是有效载荷大小
                if(pkt.x + pkt.slice_size <= 3840 * 2160 * 4) {
                    memcpy(g_screenBuf + pkt.x, pkt.data, pkt.slice_size);
                    
                    // 为了性能，我们不每包都刷新。
                    // 当收到偏移量接近“屏幕末尾”的分片时，认为一帧完成，触发重绘。
                    // 这里简化处理：每收到一个分片就标记需要重绘（Windows 会自动合并重绘请求）
                    InvalidateRect(g_mainWnd, NULL, FALSE);
                }
            }
        }
    }
    return 0;
}

// --- 主窗口回调 (处理远程画面渲染与交互) ---
LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            if(g_screenBuf) {
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = g_scrW; 
                bmi.bmiHeader.biHeight = -g_scrH; // 负值表示 Top-Down 位图
                bmi.bmiHeader.biPlanes = 1; 
                bmi.bmiHeader.biBitCount = 32;
                
                // 获取当前窗口大小进行拉伸显示
                RECT rc; GetClientRect(h, &rc);
                StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 
                              0, 0, g_scrW, g_scrH, 
                              g_screenBuf, &bmi, DIB_RGB_COLORS, SRCCOPY);
            }
            
            // --- 在光标/窗口左上角指示状态的彩色小点 ---
            HBRUSH br = CreateSolidBrush(CLR_ACTIVE);
            HGDIOBJ old = SelectObject(hdc, br);
            Ellipse(hdc, 10, 10, 25, 25); // 在固定位置画一个指示点
            SelectObject(hdc, old);
            DeleteObject(br);

            EndPaint(h, &ps);
            break;
        }
        case WM_LBUTTONDOWN: {
            // 将本地点击坐标换算为远程屏幕坐标
            RECT rc; GetClientRect(h, &rc);
            int rx = (LOWORD(l) * g_scrW) / rc.right;
            int ry = (HIWORD(l) * g_scrH) / rc.bottom;
            
            P2PPacket pkt = { AUTH_MAGIC, 1, rx, ry, 0 }; // type 1 为指令
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
            break;
        }
#if ENABLE_TRAY
        case WM_TRAY_MSG:
            if (l == WM_RBUTTONUP) {
                POINT p; GetCursorPos(&p);
                HMENU mnu = CreatePopupMenu();
                AppendMenuW(mnu, MF_STRING, ID_TRAY_EXIT, L"退出控制端 (Exit)");
                SetForegroundWindow(h);
                TrackPopupMenu(mnu, TPM_RIGHTBUTTON, p.x, p.y, 0, h, NULL);
                DestroyMenu(mnu);
            }
            break;
        case WM_COMMAND:
            if (LOWORD(w) == ID_TRAY_EXIT) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                ExitProcess(0);
            }
            break;
#endif
        case WM_DESTROY:
            if(ENABLE_TRAY) Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(h, m, w, l);
    }
    return 0;
}

// --- 登录对话框回调 ---
INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND) {
        if (LOWORD(w) == IDOK) {
            char buf[64];
            GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
            g_srv.sin_family = AF_INET;
            g_srv.sin_port = htons(P2P_PORT);
            g_srv.sin_addr.s_addr = inet_addr(buf);
            if (g_srv.sin_addr.s_addr == INADDR_NONE) {
                MessageBoxA(h, "无效的 IP 地址", "错误", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            EndDialog(h, IDOK);
            return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) {
            EndDialog(h, IDCANCEL);
            ExitProcess(0);
        }
    }
    return FALSE;
}

// --- 程序入口 ---
int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa;
    WSAStartup(0x0202, &wsa);

    // 1. 弹出登录框手动输入 IP
    if (DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) return 0;

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // 2. 注册并创建主显示窗口
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hI;
    wc.lpszClassName = L"P2P_Remote_View";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    g_mainWnd = CreateWindowW(L"P2P_Remote_View", L"Remote Desktop Control", 
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 0, 0, hI, 0);

    // 3. 启动接收线程
    CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);

    // 4. 设置托盘图标
#if ENABLE_TRAY
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_mainWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_MSG;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Control Client");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
#endif

    // 5. 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
