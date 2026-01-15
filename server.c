#include "p2p_config.h"

SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
ULONG_PTR g_gdiplusToken;

// 获取 JPEG 编码器 CLSID
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GdipGetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    GdipGetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo); return j;
        }
    }
    free(pImageCodecInfo); return -1;
}

DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScr = GetDC(NULL);
    CLSID jpgClsid;
    GetEncoderClsid(L"image/jpeg", &jpgClsid);

    int current_frame = 0;
    while (1) {
        if (!g_hasClient) { Sleep(200); continue; }

        // 1. 抓取屏幕并创建 GDI+ 位图
        GpBitmap* pBitmap = NULL;
        HDC hdcMem = CreateCompatibleDC(hdcScr);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
        SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        GdipCreateBitmapFromHBITMAP(hBmp, NULL, &pBitmap);

        // 2. 压缩为 JPEG 到内存流
        IStream* pStream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &pStream);
        GdipSaveImageToStream((GpImage*)pBitmap, pStream, &jpgClsid, NULL);

        // 3. 读取流数据并发送
        HGLOBAL hGlobal = NULL;
        GetHGlobalFromStream(pStream, &hGlobal);
        unsigned char* pJpegData = (unsigned char*)GlobalLock(hGlobal);
        int jpgSize = (int)GlobalSize(hGlobal);

        current_frame++;
        for (int i = 0; i < jpgSize; i += CHUNK_SIZE) {
            P2PPacket pkt;
            pkt.magic = AUTH_MAGIC; pkt.type = 2;
            pkt.frame_id = current_frame;
            pkt.offset = i;
            pkt.total_size = jpgSize;
            pkt.slice_size = (jpgSize - i < CHUNK_SIZE) ? (jpgSize - i) : CHUNK_SIZE;
            memcpy(pkt.data, pJpegData + i, pkt.slice_size);
            sendto(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
            if(i % (CHUNK_SIZE * 50) == 0) Sleep(1); // 拥塞控制
        }

        GlobalUnlock(hGlobal);
        pStream->lpVtbl->Release(pStream);
        GdipDisposeImage((GpImage*)pBitmap);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        Sleep(30); // 约 30 FPS
    }
    return 0;
}

// WinMain 略，只需在开头加入 GdiplusStartup，结尾 GdiplusShutdown
