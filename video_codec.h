#ifndef VIDEO_CODEC_H
#define VIDEO_CODEC_H

#include "p2p_config.h"

// 服务端使用的函数
void VideoCodec_InitSender(int w, int h);
void VideoCodec_CaptureAndSend(SOCKET s, struct sockaddr_in* dest);

// 客户端使用的函数
void VideoCodec_InitReceiver();
void VideoCodec_HandlePacket(P2PPacket* pkt);
void VideoCodec_Render(HDC hdc, HWND hwnd, int screenW, int screenH);

#endif
