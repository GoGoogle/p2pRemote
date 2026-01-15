#define NONAMELESSUNION
#include "p2p_config.h"
#include <gdiplus/gdiplusflat.h> // 使用平面接口

SOCKET g_srvSock;
struct sockaddr_in g_clientAddr;
BOOL g_hasClient = FALSE;
NOTIFYICONDATAW g_nid = { 0 };
ULONG_PTR g_gdiToken;

// 获取 JPEG 编码器
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GdipGetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    GpImageCodecInfo* pIci = (GpImageCodecInfo*)malloc(size);
    GdipGetImageEncoders(num, size, pIci);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pIci[j].MimeType, format) == 0) {
            *pClsid = pIci[j].Clsid;
            free(pIci); return j;
        }
    }
    free(pIci); return -1;
}

DWORD WINAPI ScreenThread(LPVOID lp) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScr = GetDC(NULL);
    CLSID jpgClsid;
    GetEncoderClsid(L"image/jpeg", &jpgClsid);

    int frame_id = 0;
    while (1) {
        if (!g_hasClient) { Sleep(200); continue; }

        GpBitmap* bmp = NULL;
        HDC hdcMem = CreateCompatibleDC(hdcScr);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, w, h);
        SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScr, 0, 0, SRCCOPY);
        
        GdipCreateBitmapFromHBITMAP(hBmp, NULL, &bmp);
        
        IStream* stream = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        GdipSaveImageToStream((GpImage*)bmp, stream, &jpgClsid, NULL);

        HGLOBAL hg = NULL;
        GetHGlobalFromStream(stream, &hg);
        unsigned char* data = (unsigned char*)GlobalLock(hg);
        int size = (int)GlobalSize(hg);

        frame_id++;
        for (int i = 0; i < size; i += CHUNK_SIZE) {
            P2PPacket pkt = { AUTH_MAGIC, 2, frame_id, i, size };
            pkt.slice_size = (size - i < CHUNK_SIZE) ? (size - i) : CHUNK_SIZE;
            memcpy(pkt.data, data + i, pkt.slice_size);
            sendto(g_srvSock, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&g_clientAddr, sizeof(g_clientAddr));
        }

        GlobalUnlock(hg);
        stream->lpVtbl->Release(stream);
        GdipDisposeImage((GpImage*)bmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        Sleep(40); 
    }
    return 0;
}

// 记得在 WinMain 中初始化 GDI+
// GdiplusStartupInput gsi = {1};
// GdiplusStartup(&g_gdiToken, &gsi, NULL);
