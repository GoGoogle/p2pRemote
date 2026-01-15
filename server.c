#include "p2p_config.h"
#include <d3d11.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&addr, sizeof(addr));

    P2PPacket pkt;
    struct sockaddr_in client_addr;
    int clen = sizeof(client_addr);

    while (1) {
        if (recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&client_addr, &clen) > 0) {
            if (pkt.magic == AUTH_MAGIC) {
                if (pkt.type == 2) { // 模拟点击
                    SetCursorPos(pkt.x, pkt.y);
                    mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }
            }
        }
        Sleep(10); // 降低 CPU 占用
    }
    return 0;
}
