#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H
#include "p2p_config.h"

// 服务端接口：初始化需要宽、高和用于参考的 UDP Socket
void VideoCodec_InitSender(int w, int h, SOCKET udpSock);
void VideoCodec_CaptureAndSend(SOCKET udpSock, struct sockaddr_in* dest);

// 客户端接口：初始化需要 UDP Socket 和服务端地址（用于建立 TCP 连接）
void VideoCodec_InitReceiver(SOCKET udpSock, struct sockaddr_in* srvAddr);
void VideoCodec_HandlePacket(P2PPacket* pkt);
void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH);

#endif
