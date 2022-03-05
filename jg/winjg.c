#include "jg.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

static volatile bool running;
static volatile time_t uframeTime;
static pthread_t thread_id;

static HWND Window;
static HDC BufferDc;
static HDC WindowDc;

static volatile JGCAMERA *Camera;

static int MouseX, MouseY; int JGGetMouseX(void) { return MouseX; } int JGGetMouseY(void) { return MouseY; } void JGGetMousePosition(JGPOINT *dest) { dest->x = MouseX, dest->y = MouseY; }
static int PMouseX, PMouseY; int JGGetPMouseX(void) { return PMouseX; } int JGGetPMouseY(void) { return PMouseY; } void JGGetPMousePosition(JGPOINT *dest) { dest->x = PMouseX, dest->y = PMouseY; }
static float BufferStretchX; float JGGetBufferStretchX(void) { return BufferStretchX; }
static float BufferStretchY; float JGGetBufferStretchY(void) { return BufferStretchY; }

static XFORM Transform;

void JGGetWindowSize(JGSIZE *size)
{
    RECT r;
    GetClientRect(Window, &r);
    size->width = r.right;
    size->height = r.bottom;
}

static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static void *Run(void*);

void JGInit(int argc, char **argv)
{
    __JGControlInit();

    WNDCLASS wc = {0};
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_DBLCLKS;
    wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
    wc.lpszClassName = "JGWC_MAIN_WINDOW";
    wc.lpfnWndProc = MainWndProc;
    RegisterClass(&wc);

    Window = CreateWindow("JGWC_MAIN_WINDOW", "Title", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL);
    WindowDc = GetDC(Window);

    memset((void*) (Camera = malloc(sizeof(*Camera))), 0, sizeof(*Camera));

    BufferDc = CreateCompatibleDC(WindowDc);
    SetBkMode(BufferDc, TRANSPARENT);
}

void JGSetBufferSize(int width, int height)
{
    color_t *buffer;
    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(bmi);
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biBitCount = 32;
    SelectObject(BufferDc, CreateDIBSection(WindowDc, &bmi, DIB_RGB_COLORS, (void**) &buffer, NULL, 0));
    __JGSetBuffer(buffer, width, height);
}

void JGCameraTranslate(float x, float y)
{
    Camera->x += x;
    Camera->y += y;
}

void JGSetCameraPos(float x, float y)
{
    Camera->x = x;
    Camera->y = y;
}

void JGGetCameraPos(JGPOINT2D *p)
{
    p->x = Camera->x;
    p->y = Camera->y;
}

void JGSetCamera(JGCAMERA *newCamera)
{
    Camera->x = newCamera->x;
    Camera->y = newCamera->y;
    Camera->cFlags = newCamera->cFlags;
    Camera->cstr_l = newCamera->cstr_l;
    Camera->cstr_t = newCamera->cstr_t;
    Camera->cstr_r = newCamera->cstr_r;
    Camera->cstr_b = newCamera->cstr_b;
    Camera->target = newCamera->target;
}

void JGGetCamera(JGCAMERA *camera)
{
    camera->x = Camera->x;
    camera->y = Camera->y;
    camera->cFlags = Camera->cFlags;
    camera->cstr_l = Camera->cstr_l;
    camera->cstr_t = Camera->cstr_t;
    camera->cstr_r = Camera->cstr_r;
    camera->cstr_b = Camera->cstr_b;
    camera->target = Camera->target;
}

void JGSetCameraTarget(void *p)
{
    Camera->target = p;
}

void JGResetCamera(void)
{
    Camera->x = 0;
    Camera->y = 0;
    Camera->target = NULL;
    SetWorldTransform(BufferDc, &Transform);
}

void JGTextAlign(int textAlign)
{
    SetTextAlign(BufferDc, textAlign);
}

void JGTextColor(color_t textColor)
{
    SetTextColor(BufferDc, textColor);
}

void JGFont(const char *name, int size)
{
    static HFONT lastFont = NULL;
    DeleteObject(lastFont);
    lastFont = CreateFont(size, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, DEFAULT_QUALITY, DEFAULT_PITCH, name);
    SelectObject(BufferDc, lastFont);
}

void JGSetFont(JGFONT font)
{
    SelectObject(BufferDc, font);
}

JGFONT JGCreateFont(const char *name, int size)
{
    return CreateFont(size, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, DEFAULT_QUALITY, DEFAULT_PITCH, name);
}

int JGTextWidth(string_t text, int len)
{
    SIZE s;
    GetTextExtentPoint32(BufferDc, text, len, &s);
    return s.cx;
}

int JGTextHeight(void)
{
    TEXTMETRIC tm;
    GetTextMetrics(BufferDc, &tm);
    return tm.tmAscent + tm.tmDescent;
}

void JGText(string_t text, int len, int x, int y)
{
    TextOut(BufferDc, x, y, text, len);
}

void JGResetTransform(void)
{
    Transform.eM11 = 1.0f;
    Transform.eM12 = 0.0f;
    Transform.eM21 = 0.0f;
    Transform.eM22 = 1.0f;
    Transform.eDx  = 0.0f;
    Transform.eDy  = 0.0f;
    SetWorldTransform(BufferDc, &Transform);
}

void JGTranslate(float x, float y)
{
    Transform.eDx += x * Transform.eM11 + y * Transform.eM12;
    Transform.eDy += -x * Transform.eM21 + y * Transform.eM22;
    SetWorldTransform(BufferDc, &Transform);
}

void JGScale(float x, float y)
{
    Transform.eM11 *= x;
    Transform.eM12 *= x;
    Transform.eM21 *= y;
    Transform.eM22 *= y;
    SetWorldTransform(BufferDc, &Transform);
}

void JGShear(float x, float y)
{
    double t0, t1;
    t0 = Transform.eM11;
    t1 = Transform.eM12;
    Transform.eM11 = t0 + t1 * y;
    Transform.eM12 = t0 * x + t1;

    t0 = Transform.eM21;
    t1 = Transform.eM22;
    Transform.eM21 = t0 + t1 * y;
    Transform.eM22 = t0 * x + t1;
    SetWorldTransform(BufferDc, &Transform);
}

void JGRotate(float r)
{
    double sin, cos, t0, t1;
    sincos(r, &sin, &cos);
    t0 = Transform.eM11;
    t1 = Transform.eM12;
    Transform.eM11 =  cos * t0 + sin * t1;
    Transform.eM12 = -sin * t0 + cos * t1;
    t0 = Transform.eM21;
    t1 = Transform.eM22;
    Transform.eM21 =  cos * t0 + sin * t1;
    Transform.eM22 = -sin * t0 + cos * t1;
    SetWorldTransform(BufferDc, &Transform);
}

void JGRect2D(const JGRECT2D *r)
{
    JGRect((int) (r->left - Camera->x), (int) (r->top - Camera->y), (int) (r->right - Camera->x), (int) (r->bottom - Camera->y));
}

void JGOval2D(const JGELLIPSE2D *e)
{
    JGOval((int) (e->left - Camera->x), (int) (e->top - Camera->y), (int) (e->right - Camera->x), (int) (e->bottom - Camera->y));
}

void JGLine2D(const JGLINE2D *l)
{
    JGLine((int) (l->x1 - Camera->x), (int) (l->y1 - Camera->y), (int) (l->x2 - Camera->x), (int) (l->y2 - Camera->y));
}

void JGSetFPSLimit(double limit)
{
    assert(limit >= 1e-3 && "SetFPSLimit(limit), limit must be greater than 0.001");
    uframeTime = (time_t) round(limit > 1e3 ? 1 : 1e6 / limit);
}

void JGRun(void)
{
    MSG msg;
    if(running)
        return;
    pthread_create(&thread_id, NULL, Run, NULL);
    // wait until thread started
    while(!running);
    ShowWindow(Window, SW_NORMAL);
    UpdateWindow(Window);
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void JGStop(void)
{ running = 0; SendMessage(Window, WM_CLOSE, 0, 0); }

#define SEC_TO_MICRO 1000000

static void *Run(void *arg)
{
    struct timespec start, end;
    time_t mdiff;
    time_t uruntime = 0, udiff = 0;
    time_t noverflow = 0;
    RECT wr;
    int cFlags;
    long x_align, y_align;
    double x, y, tx, ty;
    int l, t, r, b, drl, dbt;
    void *target;
    JGStart();
    running = 1;
    while(running)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);
        mdiff = udiff / 1000;
        uruntime += udiff;
        JGStep(uruntime / 1000, mdiff);
        JGAnimation_Forward(mdiff);
        GetClientRect(Window, &wr);
        // update camera if it exists
        if(Camera && (target = Camera->target))
        {
            cFlags = Camera->cFlags;
            x = Camera->x;
            y = Camera->y;
            if(cFlags & JGCA_FTARGET)
            {
                tx = ((JGPOINT2D*) target)->x;
                ty = ((JGPOINT2D*) target)->y;
            }
            else
            {
                tx = (double) ((JGPOINT*) target)->x;
                ty = (double) ((JGPOINT*) target)->y;
            }
            if(fabs(x - tx) >= 0.1d || fabs(y - ty) >= 0.1d)
            {
                x_align = (cFlags & JGCA_HCENTER) ? (JGGetBufferWidth() / 2) : (cFlags & JGCA_RIGHT) ? JGGetBufferWidth() : 0;
                y_align = (cFlags & JGCA_VCENTER) ? (JGGetBufferHeight() / 2) : (cFlags & JGCA_BOTTOM) ? JGGetBufferHeight() : 0;
                l = Camera->cstr_l;
                r = Camera->cstr_r;
                t = Camera->cstr_t;
                b = Camera->cstr_b;
                drl = r - l;
                dbt = b - t;
                tx = drl <= JGGetBufferWidth() ? 0
                        : -tx - x_align >= l - JGGetBufferWidth() ? -l
                        : x_align + tx > r ? r - JGGetBufferWidth()
                        : tx - x_align;
                ty = dbt <= JGGetBufferHeight() ? 0
                        : -ty - y_align >= t - JGGetBufferHeight() ? -t
                        : y_align + ty > b ? b - JGGetBufferHeight()
                        : ty - y_align;
                Camera->x += (tx - x) * 0.1f;
                Camera->y += (ty - y) * 0.1f;
            }
        }
        // user draw
        JGDraw();
        // draw controls
        JGDispatchEvent(JGEVENT_REDRAW, NULL);
        // stretch buffer to window
        StretchBlt(WindowDc, 0, 0, wr.right, wr.bottom, BufferDc, 0, 0, JGGetBufferWidth(), JGGetBufferHeight(), SRCCOPY);
        //BitBlt(WindowDc, 0, 0, wr.right, wr.bottom, BufferDc, 0, 0, SRCCOPY);
        clock_gettime(CLOCK_MONOTONIC, &end);
        udiff = (end.tv_sec - start.tv_sec) * SEC_TO_MICRO + (noverflow + end.tv_nsec - start.tv_nsec) / 1000;
        noverflow = (end.tv_nsec - start.tv_nsec) % 1000;
        //printf("%d\n", uoverflow);
        if(uframeTime > udiff)
        {
            usleep(uframeTime - udiff);
            udiff = uframeTime;
        }
    }
    return NULL;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    JGEVENT event; event.id = 0;
    PAINTSTRUCT ps;
    HDC hdc;
    RECT *rp;
    switch(msg)
    {
    case WM_CREATE: JGSetCursor(JGCURSOR_DEFAULT); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE:
        BufferStretchX = (float) JGGetBufferWidth() / (float) LOWORD(lParam);
        BufferStretchY = (float) JGGetBufferHeight() / (float) HIWORD(lParam);
        event.id = JGEVENT_SIZE;
        break;
    case WM_PAINT:
        rp = &ps.rcPaint;
        hdc = BeginPaint(hWnd, &ps);
        BitBlt(hdc, rp->left, rp->top, rp->right - rp->left, rp->bottom - rp->left, BufferDc, rp->left, rp->right, SRCCOPY);
        EndPaint(hWnd, &ps);
        return 0;
    case WM_SETCURSOR:
        event.id = JGEVENT_SETCURSOR;
        JGEvent(&event);
        JGDispatchEvent(JGEVENT_SETCURSOR, NULL);
        SetCursor(JGGetCursor());
        return 0;
    case WM_SETFOCUS:
        event.id = JGEVENT_FOCUSGAINED;
        JGEvent(&event);
        return 0;
    case WM_KILLFOCUS:
        event.id = JGEVENT_FOCUSLOST;
        JGEvent(&event);
        return 0;
    case WM_KEYDOWN:
        event.id = JGEVENT_KEYPRESSED;
        event.vkCode = wParam;
        event.keyChar = MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);
        event.flags = lParam;
        break;
    case WM_KEYUP:
        event.id = JGEVENT_KEYRELEASED;
        event.vkCode = wParam;
        event.keyChar = MapVirtualKey(wParam, MAPVK_VK_TO_CHAR);
        event.flags = lParam;
        break;
    case WM_CHAR:
        event.id = JGEVENT_KEYTYPED;
        event.vkCode = wParam;
        event.keyChar = wParam;
        event.flags = lParam;
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        SetCapture(hWnd);
        event.id = JGEVENT_MOUSEPRESSED;
        event.x = GET_X_LPARAM(lParam) * BufferStretchX;
        event.y = GET_Y_LPARAM(lParam) * BufferStretchY;
        event.flags = LOWORD(wParam);
        event.pressedButton = msg;
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        ReleaseCapture();
        event.id = JGEVENT_MOUSERELEASED;
        event.x = GET_X_LPARAM(lParam) * BufferStretchX;
        event.y = GET_Y_LPARAM(lParam) * BufferStretchY;
        event.flags = LOWORD(wParam);
        event.pressedButton = msg;
        break;
    case WM_MOUSEWHEEL:
        event.id = JGEVENT_MOUSEWHEEL;
        event.x = GET_X_LPARAM(lParam) * BufferStretchX;
        event.y = GET_Y_LPARAM(lParam) * BufferStretchY;
        event.flags = LOWORD(wParam);
        event.deltaWheel = GET_WHEEL_DELTA_WPARAM(wParam);
        break;
    case WM_MOUSEMOVE:
        event.id = JGEVENT_MOUSEMOVED;
        event.x = MouseX = GET_X_LPARAM(lParam) * BufferStretchX;
        event.y = MouseY = GET_Y_LPARAM(lParam) * BufferStretchY;
        event.dx = event.x - PMouseX;
        event.dy = event.y - PMouseY;
        PMouseX = event.x;
        PMouseY = event.y;
        event.flags = LOWORD(wParam);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    if(JGDispatchEvent(event.id, &event) != JGCONSUME)
        JGEvent(&event);
    return 0;
}
