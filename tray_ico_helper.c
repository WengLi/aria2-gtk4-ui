#include <gtk/gtk.h>
#include "aria2.h"
#define SHOW_WINDOW 12
#define APP_QUIT 13

LPCTSTR szAppClassName = L"aria2-gtk-ui";
LPCTSTR szAppWindowName = L"aria2-gtk-ui";
HMENU hmenu; //菜单句柄
static GtkWidget *window;
static aria2_object *aria2;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    NOTIFYICONDATAW nid;
    UINT WM_TASKBARCREATED;
    POINT pt; //用于接收鼠标坐标
    int xx;   //用于接收菜单选项返回值

    // 不要修改TaskbarCreated，这是系统任务栏自定义的消息
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    switch (message)
    {
    case WM_CREATE: //窗口创建时候的消息.
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = 0;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_USER;
        nid.hIcon = (HICON)LoadImageW(NULL, L"app.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
        lstrcpyW(nid.szTip, szAppClassName);
        Shell_NotifyIconW(NIM_ADD, &nid);
        hmenu = CreatePopupMenu();                                  //生成菜单
        AppendMenuW(hmenu, MF_STRING, SHOW_WINDOW, L"show window"); //为菜单添加两个选项
        AppendMenuW(hmenu, MF_STRING, APP_QUIT, L"quit");
        break;
    case WM_USER: //连续使用该程序时候的消息.
        if (lParam == WM_LBUTTONDBLCLK)
        {
            gtk_widget_show(window);
        }
        if (lParam == WM_RBUTTONDOWN)
        {
            GetCursorPos(&pt);         //取鼠标坐标
            SetForegroundWindow(hwnd); //解决在菜单外单击左键菜单不消失的问题
            // EnableMenuItem(hmenu, SHOW_WINDOW, MF_GRAYED);                           //让菜单中的某一项变灰
            xx = TrackPopupMenu(hmenu, TPM_RETURNCMD, pt.x, pt.y, NULL, hwnd, NULL); //显示菜单并获取选项ID
            if (xx == SHOW_WINDOW)
            {
                gtk_widget_show(window);
            }
            if (xx == APP_QUIT)
            {
                aria2_shutdown(aria2);
                gtk_window_destroy(GTK_WINDOW(window));
                SendMessageW(hwnd, WM_CLOSE, wParam, lParam);
            }
            if (xx == 0)
            {
                PostMessageW(hwnd, WM_LBUTTONDOWN, NULL, NULL);
            }
        }
        break;
    case WM_DESTROY: //窗口销毁时候的消息.
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    default:
        /*
         * 防止当Explorer.exe 崩溃以后，程序在系统系统托盘中的图标就消失
         *
         * 原理：Explorer.exe 重新载入后会重建系统任务栏。当系统任务栏建立的时候会向系统内所有
         * 注册接收TaskbarCreated 消息的顶级窗口发送一条消息，我们只需要捕捉这个消息，并重建系
         * 统托盘的图标即可。
         */
        if (message == WM_TASKBARCREATED)
            SendMessageW(hwnd, WM_CREATE, wParam, lParam);
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void *gtk_set_tray_ico(void *ptr)
{
    HWND hwnd;
    MSG msg;
    WNDCLASS wndclass;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    HWND handle = FindWindowW(NULL, szAppWindowName);
    if (handle != NULL)
    {
        MessageBoxW(NULL, L"Application is already running", szAppClassName, MB_ICONERROR);
        return 0;
    }

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppClassName;

    if (!RegisterClass(&wndclass))
    {
        MessageBoxW(NULL, L"This program requires Windows NT!", szAppClassName, MB_ICONERROR);
        return 0;
    }

    // 此处使用WS_EX_TOOLWINDOW 属性来隐藏显示在任务栏上的窗口程序按钮
    hwnd = CreateWindowEx(WS_EX_TOOLWINDOW,
                          szAppClassName, szAppWindowName,
                          WS_POPUP,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, 0);
    UpdateWindow(hwnd);
    //消息循环
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return NULL;
}

void gtk_tray_ico_thread(GtkWidget *win, aria2_object *a)
{
    window = win;
    aria2 = a;
    pthread_t pthread_ico;
    pthread_create(&pthread_ico, NULL, gtk_set_tray_ico, NULL);
}