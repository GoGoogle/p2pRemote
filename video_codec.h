#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H
#include "p2p_config.h"

// 服务端：传入 UDP 套接字用于握手，函数内部会建立 TCP 监听
void VideoCodec_InitSender(int w, int h, SOCKET udpSock);
void VideoCodec_CaptureAndSend(SOCKET udpSock, struct sockaddr_in* dest);

// 客户端：传入 UDP 套接字，函数内部会连接服务端的 TCP
void VideoCodec_InitReceiver(SOCKET udpSock, struct sockaddr_in* srvAddr);
void VideoCodec_HandlePacket(P2PPacket* pkt); // 保持兼容，但实际数据走 TCP
void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH);

#endif
