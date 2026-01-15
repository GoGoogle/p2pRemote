#include "p2p_config.h"
#include "video_codec.h"

// --- 全局变量 ---
SOCKET g_sock;
struct sockaddr_in g_srv;
HWND g_mainWnd;
NOTIFYICONDATAW g_nid = {0};
int g_scrW = 1920, g_scrH = 1080; 

// --- 接收线程 ---
DWORD WINAPI RecvThread(LPVOID lp) {
    VideoCodec_InitReceiver();
    P2PPacket pkt; struct sockaddr_in f; int fl = sizeof(f);
    
    while(1) {
        int r = recvfrom(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl);
        if(r > 0 && pkt.magic == AUTH_MAGIC && pkt.type == 2) {
            // 将分片交给公共处理函数
            VideoCodec_HandlePacket(&pkt);
            
            // 简单的帧同步：当收到接近尾部的包时刷新
            if (pkt.x + pkt.slice_size >= pkt.y - CHUNK_SIZE) {
                InvalidateRect(g_mainWnd, NULL, FALSE);
            }
        }
    }
    return 0;
}

LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            
            // 调用公共渲染函数
            VideoCodec_Render(hdc, h, g_scrW, g_scrH);
            
            // 状态指示点 (根据您的个性化要求：在光标/窗口左上角指示)
            HBRUSH br = CreateSolidBrush(CLR_ACTIVE);
            SelectObject(hdc, br);
            Ellipse(hdc, 10, 10, 25, 25); 
            DeleteObject(br);

            EndPaint(h, &ps);
            break;
        }
        case WM_LBUTTONDOWN: 
        case WM_RBUTTONDOWN: 
        case WM_LBUTTONDBLCLK: {
            RECT rc; GetClientRect(h, &rc);
            int rx = (LOWORD(l) * g_scrW) / (rc.right ? rc.right : 1);
            int ry = (HIWORD(l) * g_scrH) / (rc.bottom ? rc.bottom : 1);
            int type = (m == WM_LBUTTONDOWN) ? 1 : (m == WM_RBUTTONDOWN ? 3 : 5);
            
            P2PPacket pkt = { AUTH_MAGIC, type, rx, ry, 0 };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP: {
            P2PPacket pkt = { AUTH_MAGIC, 4, (int)w, (m == WM_KEYUP ? KEYEVENTF_KEYUP : 0), 0 };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
            break;
        }
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(h, m, w, l);
    }
    return 0;
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDOK) {
        char buf[64];
        GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
        g_srv.sin_family = AF_INET;
        g_srv.sin_port = htons(P2P_PORT);
        g_srv.sin_addr.s_addr = inet_addr(buf);
        EndDialog(h, IDOK); return TRUE;
    }
    if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) ExitProcess(0);
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA wsa; WSAStartup(0x0202, &wsa);
    if (DialogBoxParamA(hI, MAKEINTRESOURCEA(IDD_LOGIN), NULL, DlgProc, 0) != IDOK) return 0;

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc; wc.hInstance = hI; wc.lpszClassName = L"P2P_View";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS; // 支持双击检测
    RegisterClassW(&wc);

    g_mainWnd = CreateWindowW(L"P2P_View", L"Remote Desktop", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 800, 600, 0, 0, hI, 0);

    CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
