#include "p2p_config.h"

SOCKET g_sock;
struct sockaddr_in g_srv;
HWND g_mainWnd;
NOTIFYICONDATAW g_nid = {0};
unsigned char* g_screenBuf = NULL;
int g_scrW = 1920, g_scrH = 1080; 

DWORD WINAPI RecvThread(LPVOID lp) {
    g_screenBuf = (unsigned char*)malloc(3840 * 2160 * 4);
    P2PPacket pkt; struct sockaddr_in f; int fl = sizeof(f);
    while(1) {
        int r = recvfrom(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl);
        if(r > 0 && pkt.magic == AUTH_MAGIC && pkt.type == 2) {
            if(pkt.x + pkt.slice_size <= 3840 * 2160 * 4) {
                memcpy(g_screenBuf + pkt.x, pkt.data, pkt.slice_size);
                if (pkt.x + pkt.slice_size >= pkt.y - CHUNK_SIZE) InvalidateRect(g_mainWnd, NULL, FALSE);
            }
        }
    }
    return 0;
}

LRESULT CALLBACK MainWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        if(g_screenBuf) {
            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = g_scrW; bmi.bmiHeader.biHeight = -g_scrH;
            bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
            RECT rc; GetClientRect(h, &rc);
            StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, g_scrW, g_scrH, g_screenBuf, &bmi, DIB_RGB_COLORS, SRCCOPY);
        }
        HBRUSH br = CreateSolidBrush(CLR_ACTIVE); SelectObject(hdc, br);
        Ellipse(hdc, 10, 10, 25, 25); DeleteObject(br); // 状态指示点
        EndPaint(h, &ps); return 0;
    }
    if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        RECT rc; GetClientRect(h, &rc);
        int rx = (LOWORD(l) * g_scrW) / (rc.right ? rc.right : 1);
        int ry = (HIWORD(l) * g_scrH) / (rc.bottom ? rc.bottom : 1);
        int type = (m == WM_LBUTTONDOWN) ? 1 : (m == WM_RBUTTONDOWN ? 3 : 5);
        P2PPacket pkt = { AUTH_MAGIC, type, rx, ry, 0 };
        sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
    }
    if (m == WM_KEYDOWN || m == WM_KEYUP) {
        P2PPacket pkt = { AUTH_MAGIC, 4, (int)w, (m == WM_KEYUP ? KEYEVENTF_KEYUP : 0), 0 };
        sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_srv, sizeof(g_srv));
    }
    if (m == WM_TRAY_MSG && l == WM_RBUTTONUP) {
        POINT p; GetCursorPos(&p); HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出控制端");
        SetForegroundWindow(h); TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, h, NULL);
        DestroyMenu(hMenu);
    }
    if (m == WM_COMMAND && LOWORD(w) == ID_TRAY_EXIT) { Shell_NotifyIconW(NIM_DELETE, &g_nid); ExitProcess(0); }
    if (m == WM_DESTROY) { Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0); }
    return DefWindowProc(h, m, w, l);
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDOK) {
        char buf[64]; GetDlgItemTextA(h, IDC_EDIT_ID, buf, 64);
        g_srv.sin_family = AF_INET; g_srv.sin_port = htons(P2P_PORT);
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
    WNDCLASSW wc = {0}; wc.lpfnWndProc = MainWndProc; wc.hInstance = hI; wc.lpszClassName = L"P2P_View";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS; RegisterClassW(&wc);
    g_mainWnd = CreateWindowW(L"P2P_View", L"Remote Desktop Control", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 960, 540, 0, 0, hI, 0);
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_mainWnd; g_nid.uID = 1; g_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY_MSG; g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"P2P Control"); Shell_NotifyIconW(NIM_ADD, &g_nid);
    CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
