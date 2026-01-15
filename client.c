#include "p2p_config.h"

COLORREF g_clr = CLR_TRY;
struct sockaddr_in g_target;
SOCKET g_sock;

void DrawDot() {
    HDC h = GetDC(NULL); POINT p; GetCursorPos(&p);
    HBRUSH b = CreateSolidBrush(g_clr); SelectObject(h, b);
    Ellipse(h, p.x+15, p.y+15, p.x+23, p.y+23);
    DeleteObject(b); ReleaseDC(NULL, h);
}

void Probe() {
    int o = 1; setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, (char*)&o, sizeof(o));
    struct sockaddr_in ba = { AF_INET, htons(LAN_PORT), INADDR_BROADCAST };
    sendto(g_sock, "SCAN", 4, 0, (struct sockaddr*)&ba, sizeof(ba));
    for(int i=0; i<STUN_COUNT; i++) {
        char host[64]; int port; sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port);
        struct hostent* he = gethostbyname(host);
        if(he) {
            struct sockaddr_in sa = { AF_INET, htons(port) };
            memcpy(&sa.sin_addr, he->h_addr, he->h_length);
            sendto(g_sock, "STUN", 4, 0, (struct sockaddr*)&sa, sizeof(sa));
        }
    }
    struct timeval t = {0, 800000}; setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    struct sockaddr_in f; int fl = sizeof(f); char b[64];
    if(recvfrom(g_sock, b, 64, 0, (struct sockaddr*)&f, &fl) > 0) {
        g_target = f; g_target.sin_port = htons(P2P_PORT);
        g_clr = (f.sin_addr.S_un.S_un_b.s_b1 == 192) ? CLR_LAN : CLR_WAN;
    } else g_clr = CLR_ERR;
}

int main() {
    WSADATA w; WSAStartup(0x0202, &w);
    g_sock = socket(AF_INET, SOCK_DGRAM, 0); Probe();
    while(1) {
        DrawDot();
        if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT p; GetCursorPos(&p);
            P2PPacket pkt = { AUTH_MAGIC, 2, p.x, p.y };
            sendto(g_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_target, sizeof(g_target));
        }
        // 此处可添加接收服务端剪贴板逻辑 (参考 server.c 处理 type 3)
        Sleep(20);
    }
    return 0;
}
