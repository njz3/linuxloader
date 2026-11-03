// The pictures of the emulated camera (v4l2Camera.c), as windowsloader's camera for Daytona 3 chooses them:
// [Game.<short name>] CAMERA = AUTO, the PC's first camera; a camera's name, that one; PICTURE, the still picture of
// CAMERA_PICTURE (a JPG or PNG, black without one). A camera that does not open, gives no picture in 10 s or stops
// leaves the still picture in its place; one silent for 3 s shows it until its pictures come again: the game always has
// a working camera, as a cabinet needs for its link.
// The PC's camera is read by Media Foundation (loaded when needed: Windows N has none), its pictures in RGB32 turned
// into the game's YUYV at its size; the still picture is read and scaled once by WIC. Their brightness is logged (a
// dark room or black pictures), and LL_CAMERA_BMP=<file> saves one of them as it comes.

// Media Foundation's capture (MFEnumDeviceSources, the source reader) is declared for Windows 7 and later
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <wincodec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" {
#include "cameraSource.h"
#include "../../config/config.h"
#include "../../log/log.h"
}

static int frameWidth, frameHeight;
static std::vector<uint8_t> picture; // the still picture, or black
static std::vector<uint8_t> latest;  // the PC's camera's last picture
static unsigned pictureSerial = 1, latestSerial;
static bool live;          // the PC's camera gives its pictures
static bool silent;        // ... but none for 3 s: the still picture meanwhile
static bool cameraWanted;  // AUTO or a name: a PC's camera tried
static DWORD lastFrameTick, startTick;
static bool gaveUp;
static CRITICAL_SECTION lock;
static HANDLE thread;
static volatile LONG stopping;

// BT.601's limited range, the cameras' own
static void toYuyv(const uint8_t *bgrx, int pitch, int sourceWidth, int sourceHeight, uint8_t *yuyv)
{
    for (int y = 0; y < frameHeight; y++)
    {
        const uint8_t *line = bgrx + (ptrdiff_t)pitch * (y * sourceHeight / frameHeight);
        uint8_t *out = yuyv + (size_t)y * frameWidth * 2;
        for (int x = 0; x < frameWidth; x += 2)
        {
            const uint8_t *a = line + 4 * (x * sourceWidth / frameWidth);
            const uint8_t *b = line + 4 * ((x + 1) * sourceWidth / frameWidth);
            int ya = ((66 * a[2] + 129 * a[1] + 25 * a[0] + 128) >> 8) + 16;
            int yb = ((66 * b[2] + 129 * b[1] + 25 * b[0] + 128) >> 8) + 16;
            int r = (a[2] + b[2]) / 2, g = (a[1] + b[1]) / 2, bl = (a[0] + b[0]) / 2;
            int u = ((-38 * r - 74 * g + 112 * bl + 128) >> 8) + 128;
            int v = ((112 * r - 94 * g - 18 * bl + 128) >> 8) + 128;
            out[2 * x] = (uint8_t)ya;
            out[2 * x + 1] = (uint8_t)u;
            out[2 * x + 2] = (uint8_t)yb;
            out[2 * x + 3] = (uint8_t)v;
        }
    }
}

static std::wstring wide(const char *text)
{
    for (UINT codePage : {(UINT)CP_UTF8, (UINT)CP_ACP})
    {
        int length = MultiByteToWideChar(codePage, codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, text, -1, NULL, 0);
        if (length <= 0)
            continue;
        std::wstring result(length, L'\0');
        MultiByteToWideChar(codePage, 0, text, -1, &result[0], length);
        result.resize(length - 1);
        return result;
    }
    return std::wstring();
}

static std::string narrow(const wchar_t *text)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (length <= 0)
        return std::string();
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, NULL, NULL);
    result.resize(length - 1);
    return result;
}

// The brightness of a camera's picture as it comes, before the game's YUYV: the average and the brightest of R, G, B
// (every 8th pixel), to tell a dark room from black pictures
static void brightness(const uint8_t *bgrx, int pitch, int width, int height, int *average, int *brightest)
{
    unsigned long long sum = 0, count = 0;
    int top = 0;
    for (int y = 0; y < height; y += 8)
        for (int x = 0; x < width; x += 8)
        {
            const uint8_t *p = bgrx + (ptrdiff_t)pitch * y + 4 * x;
            int value = (p[0] + p[1] + p[2]) / 3;
            sum += value;
            count++;
            for (int c = 0; c < 3; c++)
                if (p[c] > top)
                    top = p[c];
        }
    *average = count ? (int)(sum / count) : 0;
    *brightest = top;
}

// LL_CAMERA_BMP=<file>: a picture of the PC's camera as it comes, saved there (32-bit BMP)
static void savePicture(const char *path, const uint8_t *bgrx, int pitch, int width, int height)
{
    FILE *file = fopen(path, "wb");
    if (!file)
    {
        log_warn("camera: LL_CAMERA_BMP %s not written", path);
        return;
    }
    uint32_t size = (uint32_t)width * height * 4;
    uint8_t header[54] = {'B', 'M'};
    uint32_t fields[] = {54 + size, 0, 54, 40, (uint32_t)width, (uint32_t)-height, 1 | (32u << 16), 0, size, 2835, 2835,
                         0, 0};
    memcpy(header + 2, fields, sizeof(fields));
    fwrite(header, 1, sizeof(header), file);
    for (int y = 0; y < height; y++)
        fwrite(bgrx + (ptrdiff_t)pitch * y, 1, (size_t)width * 4, file);
    fclose(file);
    log_warn("camera: a picture of the PC's camera saved in %s", path);
}

// A video subtype's name: its FOURCC (MJPG, YUY2, NV12...) or its number (RGB32 is 22)
static std::string formatName(const GUID &subtype)
{
    char text[16];
    const char *c = (const char *)&subtype.Data1;
    if (c[0] >= ' ' && c[0] <= 'z' && c[1] >= ' ' && c[1] <= 'z' && c[2] >= ' ' && c[2] <= 'z' && c[3] >= ' ' &&
        c[3] <= 'z')
        snprintf(text, sizeof(text), "%.4s", c);
    else
        snprintf(text, sizeof(text), "%lu", (unsigned long)subtype.Data1);
    return text;
}

static std::string typeName(IMFMediaType *type)
{
    GUID subtype = GUID_NULL;
    UINT32 w = 0, h = 0, numerator = 0, denominator = 0;
    type->GetGUID(MF_MT_SUBTYPE, &subtype);
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h);
    MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator);
    char text[64];
    snprintf(text, sizeof(text), "%s %ux%u %.1f fps", formatName(subtype).c_str(), (unsigned)w, (unsigned)h,
             denominator ? (double)numerator / denominator : 0.0);
    return text;
}

template <class T> static void release(T *&object)
{
    if (object)
        object->Release();
    object = NULL;
}

// CAMERA_PICTURE, decoded and scaled to the game's size once (Fant, as windowsloader's)
static void loadPicture(void)
{
    const char *path = getConfig()->cameraPicture;
    if (!path[0])
    {
        log_info("camera: no CAMERA_PICTURE, the emulated camera shows black");
        return;
    }
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapScaler *scaler = NULL;
    IWICFormatConverter *converter = NULL;
    std::vector<uint8_t> bgrx((size_t)frameWidth * frameHeight * 4);
    std::wstring file = wide(path);
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
                                      (void **)&factory);
    if (SUCCEEDED(result))
        result = factory->CreateDecoderFromFilename(file.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                    &decoder);
    if (SUCCEEDED(result))
        result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result))
        result = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(result))
        result = scaler->Initialize(frame, frameWidth, frameHeight, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(result))
        result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result))
        result = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGR, WICBitmapDitherTypeNone, NULL, 0,
                                       WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(result))
        result = converter->CopyPixels(NULL, frameWidth * 4, (UINT)bgrx.size(), bgrx.data());
    release(converter);
    release(scaler);
    release(frame);
    release(decoder);
    release(factory);
    if (FAILED(result))
    {
        log_warn("camera: CAMERA_PICTURE %s not read (0x%08lX), the emulated camera shows black", path,
                 (unsigned long)result);
        return;
    }
    std::vector<uint8_t> yuyv((size_t)frameWidth * frameHeight * 2);
    toYuyv(bgrx.data(), frameWidth * 4, frameWidth, frameHeight, yuyv.data());
    EnterCriticalSection(&lock);
    picture.swap(yuyv);
    pictureSerial++;
    LeaveCriticalSection(&lock);
    log_info("camera: the emulated camera shows %s", path);
}

typedef HRESULT(WINAPI *MFStartupFunction)(ULONG, DWORD);
typedef HRESULT(WINAPI *MFShutdownFunction)(void);
typedef HRESULT(WINAPI *MFCreateAttributesFunction)(IMFAttributes **, UINT32);
typedef HRESULT(WINAPI *MFCreateMediaTypeFunction)(IMFMediaType **);
typedef HRESULT(WINAPI *MFEnumDeviceSourcesFunction)(IMFAttributes *, IMFActivate ***, UINT32 *);
typedef HRESULT(WINAPI *MFCreateSourceReaderFromMediaSourceFunction)(IMFMediaSource *, IMFAttributes *,
                                                                      IMFSourceReader **);

// The PC's camera chosen by CAMERA, read until the loader quits or it fails
static void readCamera(void)
{
    const char *wanted = getConfig()->camera;
    bool automatic = !wanted[0] || _stricmp(wanted, "AUTO") == 0;
    HMODULE mfplat = LoadLibraryA("mfplat.dll"), mf = LoadLibraryA("mf.dll"), mfreadwrite = LoadLibraryA("mfreadwrite.dll");
    MFStartupFunction startup = mfplat ? (MFStartupFunction)(void *)GetProcAddress(mfplat, "MFStartup") : NULL;
    MFShutdownFunction shutdown = mfplat ? (MFShutdownFunction)(void *)GetProcAddress(mfplat, "MFShutdown") : NULL;
    MFCreateAttributesFunction createAttributes =
        mfplat ? (MFCreateAttributesFunction)(void *)GetProcAddress(mfplat, "MFCreateAttributes") : NULL;
    MFCreateMediaTypeFunction createMediaType =
        mfplat ? (MFCreateMediaTypeFunction)(void *)GetProcAddress(mfplat, "MFCreateMediaType") : NULL;
    MFEnumDeviceSourcesFunction enumDeviceSources =
        mf ? (MFEnumDeviceSourcesFunction)(void *)GetProcAddress(mf, "MFEnumDeviceSources") : NULL;
    MFCreateSourceReaderFromMediaSourceFunction createReader =
        mfreadwrite ? (MFCreateSourceReaderFromMediaSourceFunction)(void *)GetProcAddress(
                          mfreadwrite, "MFCreateSourceReaderFromMediaSource")
                    : NULL;
    if (!startup || !shutdown || !createAttributes || !createMediaType || !enumDeviceSources || !createReader ||
        FAILED(startup(MF_VERSION, MFSTARTUP_LITE)))
    {
        log_warn("camera: no Media Foundation on this PC, the emulated camera");
        return;
    }

    IMFAttributes *attributes = NULL, *readerAttributes = NULL;
    IMFActivate **devices = NULL;
    UINT32 count = 0;
    IMFMediaSource *source = NULL;
    IMFSourceReader *reader = NULL;
    IMFMediaType *type = NULL;
    std::string name;
    if (SUCCEEDED(createAttributes(&attributes, 1)) &&
        SUCCEEDED(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)))
        enumDeviceSources(attributes, &devices, &count);
    int chosen = -1;
    for (UINT32 i = 0; i < count; i++)
    {
        WCHAR *friendly = NULL;
        UINT32 length = 0;
        std::string deviceName;
        if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendly, &length)))
        {
            deviceName = narrow(friendly);
            CoTaskMemFree(friendly);
        }
        log_info("camera: the PC's camera %u, \"%s\"", (unsigned)i, deviceName.c_str());
        if (chosen < 0 && (automatic || _stricmp(deviceName.c_str(), wanted) == 0))
            chosen = (int)i, name = deviceName;
    }
    if (chosen < 0)
        log_warn(automatic ? "camera: no camera on this PC, the emulated one"
                           : "camera: no camera \"%s\" on this PC, the emulated one",
                 wanted);
    else if (FAILED(devices[chosen]->ActivateObject(IID_IMFMediaSource, (void **)&source)))
        log_warn("camera: the PC's camera \"%s\" did not open, the emulated one", name.c_str());
    for (UINT32 i = 0; i < count; i++)
        devices[i]->Release();
    CoTaskMemFree(devices);
    release(attributes);

    // Its own pictures at the game's size when it has them, then RGB32 through the reader's video processing
    UINT32 width = 0, height = 0;
    LONG stride = 0;
    bool ready = false;
    std::string native = "its own";
    if (source && SUCCEEDED(createAttributes(&readerAttributes, 1)) &&
        SUCCEEDED(readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE)) &&
        SUCCEEDED(createReader(source, readerAttributes, &reader)))
    {
        bool matched = false;
        for (DWORD i = 0; SUCCEEDED(reader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &type)); i++)
        {
            UINT32 w = 0, h = 0;
            log_info("camera: the PC's camera \"%s\", picture type %u: %s", name.c_str(), (unsigned)i,
                     typeName(type).c_str());
            bool match = !matched && SUCCEEDED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h)) &&
                         (int)w == frameWidth && (int)h == frameHeight &&
                         SUCCEEDED(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type));
            if (match)
                matched = true, native = typeName(type);
            release(type);
        }
        if (SUCCEEDED(createMediaType(&type)) && SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
            SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)) &&
            SUCCEEDED(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type)))
        {
            release(type);
            if (SUCCEEDED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type)) &&
                SUCCEEDED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height)) && width && height)
            {
                UINT32 defaultStride = 0;
                stride = SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &defaultStride)) ? (LONG)(INT32)defaultStride
                                                                                         : (LONG)width * 4;
                ready = true;
            }
        }
        release(type);
        if (!ready)
            log_warn("camera: the PC's camera \"%s\" gives no RGB32 pictures, the emulated one", name.c_str());
    }
    release(readerAttributes);
    if (ready)
        log_warn("camera: the PC's camera \"%s\", %ux%u for the game's %dx%d (read as %s)", name.c_str(), (unsigned)width,
                 (unsigned)height, frameWidth, frameHeight, native.c_str());

    std::vector<uint8_t> yuyv((size_t)frameWidth * frameHeight * 2);
    int failures = 0;
    bool first = true;
    // Its brightness logged at its first picture, 5 s later, then every minute; LL_CAMERA_BMP saved 5 s in
    const char *bmp = getenv("LL_CAMERA_BMP");
    bool saved = !bmp || !bmp[0];
    DWORD firstTick = 0, nextReport = 0;
    unsigned pictures = 0;
    while (ready && !stopping)
    {
        DWORD stream = 0, flags = 0;
        LONGLONG time = 0;
        IMFSample *sample = NULL;
        HRESULT result = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream, &flags, &time, &sample);
        if (FAILED(result) || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)))
        {
            log_warn("camera: the PC's camera \"%s\" stopped (0x%08lX), the emulated one from now on", name.c_str(),
                     (unsigned long)result);
            release(sample);
            break;
        }
        IMFMediaBuffer *buffer = NULL;
        IMF2DBuffer *buffer2d = NULL;
        BYTE *data = NULL;
        LONG pitch = stride;
        DWORD length = 0;
        bool copied = false;
        DWORD now = GetTickCount();
        bool report = pictures == 0 || (int)(now - nextReport) >= 0;
        bool save = !saved && pictures && now - firstTick >= 5000;
        int average = 0, brightest = 0;
        // Each picture's first line and pitch (a negative stride: the picture from its last line up)
        auto use = [&](const uint8_t *picture, LONG picturePitch) {
            toYuyv(picture, picturePitch, (int)width, (int)height, yuyv.data());
            if (report)
                brightness(picture, picturePitch, (int)width, (int)height, &average, &brightest);
            if (save)
                savePicture(bmp, picture, picturePitch, (int)width, (int)height), saved = true;
            copied = true;
        };
        if (sample && SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)))
        {
            if (SUCCEEDED(buffer->QueryInterface(IID_IMF2DBuffer, (void **)&buffer2d)) &&
                SUCCEEDED(buffer2d->Lock2D(&data, &pitch)))
            {
                use(data, pitch);
                buffer2d->Unlock2D();
            }
            else if (SUCCEEDED(buffer->Lock(&data, NULL, &length)))
            {
                if (length >= (DWORD)(labs(stride) * height))
                    use(stride < 0 ? data + (size_t)(-stride) * (height - 1) : data, stride);
                buffer->Unlock();
            }
        }
        if (copied)
        {
            if (!pictures)
                firstTick = now;
            pictures++;
            if (report)
            {
                log_warn("camera: the PC's camera \"%s\", picture %u: brightness %d on average, %d at most (of 255)",
                         name.c_str(), pictures, average, brightest);
                nextReport = now + (pictures == 1 ? 5000 : 60000);
            }
        }
        release(buffer2d);
        release(buffer);
        release(sample);
        if (!copied)
        {
            if (sample && ++failures <= 3)
                log_warn("camera: the PC's camera \"%s\" gave a picture not read", name.c_str());
            continue;
        }
        EnterCriticalSection(&lock);
        DWORD gap = 0;
        if (!gaveUp)
        {
            latest.assign(yuyv.begin(), yuyv.end());
            latestSerial++;
            if (silent)
                gap = now - lastFrameTick;
            live = true;
            silent = false;
            lastFrameTick = now;
        }
        bool over = gaveUp;
        LeaveCriticalSection(&lock);
        if (over)
            break;
        if (gap)
            log_warn("camera: the PC's camera \"%s\" gives its pictures again, after %lu ms without", name.c_str(),
                     (unsigned long)gap);
        if (first)
            log_warn("camera: the PC's camera \"%s\" gives its pictures", name.c_str());
        first = false;
    }
    release(reader);
    if (source)
        source->Shutdown();
    release(source);
    shutdown();
}

static DWORD WINAPI cameraThread(LPVOID parameter)
{
    (void)parameter;
    HRESULT com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    loadPicture();
    if (cameraWanted)
        readCamera();
    EnterCriticalSection(&lock);
    live = false;
    gaveUp = true; // its end already logged
    LeaveCriticalSection(&lock);
    if (SUCCEEDED(com))
        CoUninitialize();
    return 0;
}

void cameraSourceStart(int width, int height)
{
    if (thread)
        return;
    InitializeCriticalSection(&lock);
    frameWidth = width;
    frameHeight = height;
    picture.resize((size_t)width * height * 2);
    for (size_t i = 0; i < picture.size(); i += 2)
        picture[i] = 16, picture[i + 1] = 128; // black
    const char *mode = getConfig()->camera;
    cameraWanted = _stricmp(mode, "PICTURE") != 0;
    startTick = GetTickCount();
    log_warn("camera: emulated as /dev/video0, %dx%d YUYV, CAMERA = %s, CAMERA_PICTURE = %s", width, height,
             mode[0] ? mode : "AUTO", getConfig()->cameraPicture[0] ? getConfig()->cameraPicture : "(black)");
    thread = CreateThread(NULL, 0, cameraThread, NULL, 0, NULL);
}

int cameraSourceFrame(uint8_t *yuyv, unsigned *serial)
{
    if (!thread)
        return 0;
    EnterCriticalSection(&lock);
    DWORD now = GetTickCount();
    // No picture 10 s from the start (a camera may take that long to open): the still picture, from then on. None for
    // 3 s since the last one: the still picture until the next one
    if (cameraWanted && !gaveUp && !live && now - startTick > 10000)
    {
        gaveUp = true;
        log_warn("camera: the PC's camera gives no picture, the emulated one from now on");
    }
    else if (live && !silent && now - lastFrameTick > 3000)
    {
        silent = true;
        log_warn("camera: the PC's camera gives no picture for 3 s, the emulated one until it gives them again");
    }
    bool showLive = live && !silent;
    const std::vector<uint8_t> &source = showLive ? latest : picture;
    unsigned current = showLive ? latestSerial : 0x80000000u | pictureSerial;
    memcpy(yuyv, source.data(), source.size());
    int changed = current != *serial;
    *serial = current;
    LeaveCriticalSection(&lock);
    return changed;
}

void cameraSourceStop(void)
{
    if (!thread)
        return;
    InterlockedExchange(&stopping, 1);
}
