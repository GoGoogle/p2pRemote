#include "p2p_config.h"

// 纯 STUN 解析获取外网 IP
void GetPublicIPViaStun() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    int timeout = 1500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    char public_ip[32] = "探测失败";
    
    for(int i=0; i < STUN_COUNT; i++) {
        char host[64]; int port;
        sscanf(STUN_SERVERS[i], "%[^:]:%d", host, &port);
        struct hostent* he = gethostbyname(host);
        if(!he) continue;

        struct sockaddr_in sa = { AF_INET, htons(port) };
        memcpy(&sa.sin_addr, he->h_addr, he->h_length);

        StunHeader req = { htons(0x0001), 0, htonl(0x2112A442) };
        sendto(s, (char*)&req, sizeof(req), 0, (struct sockaddr*)&sa, sizeof(sa));

        char buf[512]; struct sockaddr_in from; int flen = sizeof(from);
        int len = recvfrom(s, buf, 512, 0, (struct sockaddr*)&from, &flen);
        if (len > 20) {
            unsigned char* ptr = (unsigned char*)buf + 20;
            while(ptr < (unsigned char*)buf + len) {
                unsigned short attr_type = ntohs(*(unsigned short*)ptr);
                unsigned short attr_len = ntohs(*(unsigned short*)(ptr + 2));
                if (attr_type == 0x0020) { // XOR-MAPPED-ADDRESS
                    unsigned char* addr_ptr = ptr + 8;
                    unsigned int x_ip = *(unsigned int*)addr_ptr ^ htonl(0x2112A442);
                    struct in_addr in; in.S_un.S_addr = x_ip;
                    strcpy(public_ip, inet_ntoa(in));
                    goto found;
                }
                ptr += (4 + attr_len);
            }
        }
    }
found:
    closesocket(s);
    char msg[128];
    sprintf(msg, "纯 STUN 探测完成！\n公网 IP: %s\n请告知控制端。", public_ip);
    MessageBoxA(NULL, msg, "服务端上线", MB_OK | MB_ICONINFORMATION);
}

DWORD WINAPI LanResp(LPVOID lp) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(LAN_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&a, sizeof(a));
    char b[64]; struct sockaddr_in c; int cl = sizeof(c);
    while(1) {
        if(recvfrom(s, b, 64, 0, (struct sockaddr*)&c, &cl) > 0)
            sendto(s, "P2P_ACK", 7, 0, (struct sockaddr*)&c, cl);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    GetPublicIPViaStun();
    CreateThread(NULL, 0, LanResp, NULL, 0, NULL);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in srv = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&srv, sizeof(srv));

    P2PPacket pkt; struct sockaddr_in f; int fl = sizeof(f);
    while(1) {
        if(recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&f, &fl) > 0) {
            if(pkt.magic == AUTH_MAGIC) {
                SetCursorPos(pkt.x, pkt.y);
                if(pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
        Sleep(10);
    }
    return 0;
}
