// Copyright 2018 (C) Linus Yang
// Copyright 2011 (C) Etienne Dechamps (e-t172) <e-t172@akegroup.org>
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//x86_64-w64-mingw32-gcc -s lidlock.c -o lidlock.exe -lole32 -lksguid -static -O2 -g0 -mwindows -Wall

#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <dbt.h>

#ifdef DEBUG
static FILE *logfile = NULL;
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define USE_LOGFILE(ident)                                     \
    do {                                                       \
        if (ident != NULL) { logfile = fopen(ident, "w+"); } } \
    while (0)
#define CLOSE_LOGFILE()                             \
    do {                                            \
        if (logfile != NULL) { fclose(logfile); } } \
    while (0)
#define LOG(format, ...)                                                         \
    do {                                                                         \
        if (logfile != NULL) {                                                   \
            time_t now = time(NULL);                                             \
            char timestr[20];                                                    \
            strftime(timestr, 20, TIME_FORMAT, localtime(&now));                 \
            fprintf(logfile, " [%s] " format "\n", timestr, ## __VA_ARGS__);     \
            fflush(logfile); }                                                   \
    }                                                                            \
    while (0)
#define GUID_LOG(fmt, guid) \
    do { \
        wchar_t guid_str[64] = {0}; \
        char str[128] = {0}; \
        StringFromGUID2(&guid, (LPOLESTR)guid_str, sizeof(guid_str)); \
        WideCharToMultiByte(CP_ACP, 0, guid_str, -1, str, sizeof(str), NULL, NULL); \
        LOG(fmt, str); \
    } while(0)

static void systemError(const char *s)
{
    char *msg = NULL;
    DWORD err = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msg, 0, NULL);
    if (msg != NULL) {
        // Remove trailing newline character
        ssize_t len = strlen(msg) - 1;
        if (len >= 0 && msg[len] == '\n') {
            msg[len] = '\0';
        }
        LOG("%s: [%ld] %s", s, err, msg);
        LocalFree(msg);
    }
}
#else
#define LOG(format, ...)
#define GUID_LOG(fmt, guid)
#define USE_LOGFILE(ident)
#define CLOSE_LOGFILE()
#define systemError(s)
#endif

#define GUID_EQ(a, b) IsEqualGUID(&a, &b)
#define SINGLETON_IDENTIFIER "Global\\{3DA16D16-5F02-4CFD-8C43-11C31127889D}"
#define APP_NAME "lidlock"
static const GUID GUID_DEVINTERFACE_MONITOR = {
    0xe6f07b5f, 0xee97, 0x4a90,
    {0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7}
};

static int displayCount()
{
    int ret = 0;
    DWORD deviceNum = 0;
    DISPLAY_DEVICE dd;
    dd.cb = sizeof(DISPLAY_DEVICE);
    while (EnumDisplayDevices(NULL, deviceNum, &dd, 0)){
        DISPLAY_DEVICE ddmon = {0};
        ddmon.cb = sizeof(DISPLAY_DEVICE);
        DWORD monitorNum = 0;
        while (EnumDisplayDevices(dd.DeviceName, monitorNum, &ddmon, 0)) {
            monitorNum++;
            ret++;
        }
        deviceNum++;
    }
    return ret;
}

static HWND createWindow(HINSTANCE instance)
{
    LOG("Creating window");
    HWND hWnd = CreateWindow(
        APP_NAME,
        NULL,
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        HWND_MESSAGE,
        NULL,
        instance,
        NULL
    );

    if (hWnd == NULL) {
        systemError("creating window");
    }
    
    return hWnd;
}

static void registerNotification(HWND window)
{
    GUID_LOG("Registering GUID_MONITOR_POWER_ON (GUID: %s)", GUID_MONITOR_POWER_ON);
    if (!RegisterPowerSettingNotification(window,
          &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE)) {
        systemError("cannot register GUID_MONITOR_POWER_ON power setting notification");
    }
    GUID_LOG("Registering GUID_LIDSWITCH_STATE_CHANGE (GUID: %s)", GUID_LIDSWITCH_STATE_CHANGE);
    if (!RegisterPowerSettingNotification(window,
          &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE)) {
        systemError("cannot register GUID_LIDSWITCH_STATE_CHANGE power setting notification");
    }
    GUID_LOG("Registering GUID_DEVINTERFACE_MONITOR (GUID: %s)", GUID_DEVINTERFACE_MONITOR);
    DEV_BROADCAST_DEVICEINTERFACE filter;
    memset(&filter, 0 ,sizeof(filter));
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    filter.dbcc_classguid = GUID_DEVINTERFACE_MONITOR;
    if (!RegisterDeviceNotification(window,
          &filter, DEVICE_NOTIFY_WINDOW_HANDLE)) {
        systemError("cannot register device notification");
    }
}

static WPARAM messageLoop()
{
    for (;;)
    {
        LOG("Awaiting next window message");
        MSG message;
        BOOL result = GetMessage(&message, NULL, 0, 0);
        if (result == -1)
            systemError("getting window message");
        if (result == 0)
            return message.wParam;
        LOG("Dispatching message");
        DispatchMessage(&message);
    }
}

static LRESULT CALLBACK windowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int isPowerChanged = 0;
    if (uMsg == WM_POWERBROADCAST && wParam == PBT_POWERSETTINGCHANGE) {
        LOG("Power status changed");
        isPowerChanged = 1;
    } else if (uMsg == WM_DEVICECHANGE && wParam == DBT_DEVICEREMOVECOMPLETE) {
        LOG("Monitors removed");
    } else {
        LOG("Window received irrelevant message");
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    if (GetSystemMetrics(SM_REMOTESESSION)) {
        LOG("Ignoring window message because session is currently remote");
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    if (isPowerChanged) {
        POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING *)lParam;
        GUID_LOG("Received POWERBROADCAST_SETTING %s", setting->PowerSetting);
        if (!GUID_EQ(setting->PowerSetting, GUID_MONITOR_POWER_ON) && 
            !GUID_EQ(setting->PowerSetting, GUID_LIDSWITCH_STATE_CHANGE)) {
            LOG("Received irrelevant POWERBROADCAST_SETTING");
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }

        DWORD* state = (DWORD *)&setting->Data;
        LOG("POWERBROADCAST_SETTING state: %d", (int)*state);
        if (*state != 0) {
            LOG("Irrelevant POWERBROADCAST_SETTING state");
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
    }

    if (displayCount() == 0) {
        LOG("Locking");
        if (LockWorkStation() == 0)
            systemError("locking workstation");
        else
            LOG("Locked");
    } else {
        LOG("Still have active monitors, ignored locking");
    }
    
    return 0;
}

static void registerWindowClass(HINSTANCE instance)
{
    LOG("Registering window class");
    WNDCLASSEX windowClass = { 0 };

    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.lpfnWndProc = &windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = APP_NAME;

    if (RegisterClassEx(&windowClass) == 0) {
        systemError("registering window class");
    }
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int cmdShow)
{
    // Detect if an instance is already running
    WPARAM ret = 0;
    HANDLE mutex = CreateMutex(NULL, FALSE, TEXT(SINGLETON_IDENTIFIER));
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
        goto cleanup;
    }

    // Initialize log file
    if (*commandLine != 0) {
        USE_LOGFILE(commandLine);
    }

    // Register notifications
    registerWindowClass(instance);
    HWND window = createWindow(instance);
    registerNotification(window);
    ret = messageLoop();

    // Finished
    CLOSE_LOGFILE();

cleanup:
    if (mutex) {
        CloseHandle(mutex);
        mutex = NULL;
    }

    return ret;
}
