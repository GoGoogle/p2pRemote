#include "p2p_config.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DWORD last_clip_seq = 0;
struct sockaddr_in g_client;
SOCKET g_srv_sock;

// 剪贴板读取与同步
void SyncClipboardToServer() {
    DWORD seq = GetClipboardSequenceNumber();
    if (seq == last_clip_seq) return;
    last_clip_seq = seq;

    if (OpenClipboard(NULL)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            wchar_t* p = (wchar_t*)GlobalLock(h);
            if (p) {
                P2PPacket pkt = { AUTH_MAGIC, 3 };
                pkt.data_len = WideCharToMultiByte(CP_UTF8, 0, p, -1, pkt.data, 1024, NULL, NULL);
                SecureXOR(pkt.data, pkt.data_len); // 加密密信内容
                sendto(g_srv_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_client, sizeof(g_client));
                GlobalUnlock(h);
            }
        }
        CloseClipboard();
    }
}

DWORD WINAPI LanResp(LPVOID lp) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(LAN_PORT), INADDR_ANY };
    bind(s, (struct sockaddr*)&a, sizeof(a));
    char b[64]; struct sockaddr_in c; int cl = sizeof(c);
    while(1) {
        if(recvfrom(s, b, 64, 0, (struct sockaddr*)&c, &cl) > 0) {
            sendto(s, "P2P_SRV", 7, 0, (struct sockaddr*)&c, cl);
            system("tscon 1 /dest:console"); // 自动突破锁屏
        }
    }
    return 0;
}

int main() {
    WSADATA w; WSAStartup(0x0202, &w);
    CreateThread(NULL, 0, LanResp, NULL, 0, NULL);
    g_srv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { AF_INET, htons(P2P_PORT), INADDR_ANY };
    bind(g_srv_sock, (struct sockaddr*)&a, sizeof(a));

    P2PPacket pkt; int cl = sizeof(g_client);
    while(1) {
        if(recvfrom(g_srv_sock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_client, &cl) > 0) {
            if(pkt.magic != AUTH_MAGIC) continue;
            if(pkt.type == 2) mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            if(pkt.type == 1) SetCursorPos(pkt.x, pkt.y);
            if(pkt.type == 3) { // 接收客户端剪贴板
                SecureXOR(pkt.data, pkt.data_len);
                if(OpenClipboard(NULL)) {
                    EmptyClipboard();
                    int wl = MultiByteToWideChar(CP_UTF8, 0, pkt.data, -1, NULL, 0);
                    HGLOBAL hm = GlobalAlloc(GMEM_MOVEABLE, wl * 2);
                    wchar_t* pm = (wchar_t*)GlobalLock(hm);
                    MultiByteToWideChar(CP_UTF8, 0, pkt.data, -1, pm, wl);
                    GlobalUnlock(hm); SetClipboardData(CF_UNICODETEXT, hm); CloseClipboard();
                }
            }
        }
        SyncClipboardToServer();
        Sleep(10);
    }
    return 0;
}
