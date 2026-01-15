#include "p2p_config.h"

struct sockaddr_in g_client;
SOCKET g_srv_sock;

DWORD WINAPI LanResp(LPVOID lp) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(LAN_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&a, sizeof(a));
    char b[64]; struct sockaddr_in c; int cl = sizeof(c);
    while(1) {
        if(recvfrom(s, b, 64, 0, (struct sockaddr*)&c, &cl) > 0) {
            sendto(s, "P2P_SRV", 7, 0, (struct sockaddr*)&c, cl);
            system("tscon 1 /dest:console");
        }
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    CreateThread(NULL, 0, LanResp, NULL, 0, NULL);
    g_srv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srv_sock, (struct sockaddr*)&a, sizeof(a));

    P2PPacket pkt; int cl = sizeof(g_client);
    while(1) {
        if(recvfrom(g_srv_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_client, &cl) > 0) {
            if(pkt.magic == AUTH_MAGIC) {
                if(pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_LEFTUP,0,0,0,0);
                if(pkt.type == 1) SetCursorPos(pkt.x, pkt.y);
            }
        }
        Sleep(10);
    }
    return 0;
}
