#include "p2p_config.h"
#include <wininet.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")

// 获取并显示公网 IP
void ReportIP() {
    HINTERNET hInt = InternetOpenA("P2P", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInt, "http://api.ipify.org", NULL, 0, 0, 0);
    char ip[32] = {0}; DWORD read;
    if (hUrl) {
        InternetReadFile(hUrl, ip, 31, &read);
        char msg[128];
        sprintf(msg, "服务端已上线！\n公网 IP: %s\n请告知控制端此地址。", ip);
        MessageBoxA(NULL, msg, "服务状态", MB_OK | MB_ICONINFORMATION);
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInt);
}

// 响应局域网扫描
DWORD WINAPI LanResponder(LPVOID lp) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = { AF_INET, htons(LAN_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    char buf[64]; struct sockaddr_in client; int clen = sizeof(client);
    while(1) {
        if(recvfrom(s, buf, 64, 0, (struct sockaddr*)&client, &clen) > 0) {
            sendto(s, "P2P_ACK", 7, 0, (struct sockaddr*)&client, clen);
        }
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lp, int nS) {
    WSADATA w; WSAStartup(0x0202, &w);
    
    // 1. 弹出 IP 告知窗口
    ReportIP();
    
    // 2. 启动局域网发现线程
    CreateThread(NULL, 0, LanResponder, NULL, 0, NULL);

    // 3. 主指令监听循环
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in srv_addr = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&srv_addr, sizeof(srv_addr));

    P2PPacket pkt; struct sockaddr_in from; int flen = sizeof(from);
    while(1) {
        if(recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&from, &flen) > 0) {
            if(pkt.magic == AUTH_MAGIC) {
                SetCursorPos(pkt.x, pkt.y);
                if(pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
        Sleep(10);
    }
    return 0;
}
