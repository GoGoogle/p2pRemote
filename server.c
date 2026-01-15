#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_target;
SOCKET g_sock;
char g_input_target[64] = {0};

void DrawDot() {
    HDC h = GetDC(NULL); POINT p; GetCursorPos(&p);
    HBRUSH b = CreateSolidBrush(g_clr); SelectObject(h, b);
    Ellipse(h, p.x+15, p.y+15, p.x+23, p.y+23);
    DeleteObject(b); ReleaseDC(NULL, h);
}

void Probe(char* target) {
    struct hostent* he = gethostbyname(target);
    if(he) {
        g_target.sin_family = AF_INET;
        g_target.sin_port = htons(P2P_PORT);
        memcpy(&g_target.sin_addr, he->h_addr, he->h_length);
        g_clr = CLR_WAN;
    } else g_clr = CLR_ERR;
}

INT_PTR CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if(m == WM_COMMAND) {
        if(LOWORD(w) == IDOK) {
            GetDlgItemTextA(h, 101, g_input_target, 64);
            EndDialog(h, IDOK);
        } else if(LOWORD(w) == IDCANCEL) EndDialog(h, IDCANCEL);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    if(DialogBoxA(hI, (LPCSTR)MAKEINTRESOURCE(0), NULL, DlgProc) != IDOK) return 0;

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    Probe(g_input_target);

    while(1) {
        DrawDot();
        if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT p; GetCursorPos(&p);
            P2PPacket pkt = { AUTH_MAGIC, 2, p.x, p.y };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_target, sizeof(g_target));
        }
        if(GetAsyncKeyState(VK_ESCAPE)) break;
        Sleep(20);
    }
    return 0;
}
