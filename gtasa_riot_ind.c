/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <tlhelp32.h>

// http://stackoverflow.com/questions/865152/how-can-i-get-a-process-handle-by-its-name-in-c

HANDLE GetProcessByName(TCHAR* name)
{
    DWORD pid = 0;

    // Create toolhelp snapshot.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 process;
    ZeroMemory(&process, sizeof(process));
    process.dwSize = sizeof(process);

    // Walkthrough all processes.
    if (Process32First(snapshot, &process))
    {
        do
        {
            // Compare process.szExeFile based on format of name, i.e., trim file path
            // trim .exe if necessary, etc.
            if (stricmp(process.szExeFile, name) == 0)
            {
                pid = process.th32ProcessID;
                break;
            }
        } while(Process32Next(snapshot, &process));
    }

    CloseHandle(snapshot);

    if (pid != 0)
    {
        return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    }

    // Not found


    return NULL;
}

HWND hwnd;
BYTE cheats[92];

typedef struct {
    volatile BYTE c14;
    volatile BYTE c31;
    volatile BYTE c69;
    volatile BYTE invalid;
} riot_cheats;

riot_cheats cur_cheats = {0, 0, 0, 1};
riot_cheats prev_cheats = {0, 0, 0, 1};
DWORD last_change_t;
volatile BYTE visible;

void write_txt() {
    if (memcmp(&cur_cheats, &prev_cheats, sizeof(riot_cheats)) == 0) {
        if (visible && GetTickCount() - last_change_t > 10000) {
            visible = 0;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return;
    }
    memcpy(&prev_cheats, &cur_cheats, sizeof(riot_cheats));
    last_change_t = GetTickCount();
    visible = 1;
    InvalidateRect(hwnd, NULL, TRUE);
}

DWORD WINAPI dc_thread(LPVOID lpParameter) {
    HANDLE proc;
    do {
        Sleep(250);
        while(
            (proc = GetProcessByName("gta_sa.exe")) &&
            ReadProcessMemory(proc, (LPCVOID)0x969130, &cheats, sizeof(cheats), NULL)
        ) {
            CloseHandle(proc);
            cur_cheats.invalid = 0;
            for(int i = 0; i < sizeof(cheats); i++)
                if (cheats[i] > 1) {
                    cur_cheats.invalid = 1;
                    break;
                }

            cur_cheats.c14 = cheats[14];
            cur_cheats.c31 = cheats[31];
            cur_cheats.c69 = cheats[69];

            write_txt();
            Sleep(250);
        }
        if (proc) CloseHandle(proc);
        cur_cheats.invalid = 1;
        write_txt();
    } while(1);
}

void txtoutl(LPRECT r, HDC hDC, COLORREF fg, COLORREF bg, LPCTSTR txt, UINT format)
{
    int o[]= {-2, -1, 1, 2, 0};
    for(int x=0; x<5; x++) {
        for(int y=0; y<5; y++) {
            RECT rc;
            SetRect(&rc, r->left + o[x] + 2, r->top + o[y] + 2, r->right, r->bottom);
            SetTextColor(hDC, o[x]==0&&o[y]==0?fg:bg);
            DrawText(hDC,txt,strlen(txt),&rc,format);
        }
    }
}

void drawcheatstat(LPRECT r, HDC hDC, int num, CHAR enabled, LPSTR name)
{
    static HFONT smallf;
    if (!smallf) {
        smallf = CreateFont(22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    }
    COLORREF g_white = RGB(255, 255, 255);
    COLORREF g_black = RGB(1, 1, 1);
    COLORREF g_red = RGB(255, 0, 0);
    COLORREF g_green = RGB(0, 255, 0);
    RECT rc;

    SetRect(&rc, 0, r->top, r->right, r->bottom);
    HFONT hfOld = SelectObject(hDC, smallf);
    txtoutl(&rc, hDC, g_white, g_black, name, DT_CENTER);
    r->top += 25;
    SelectObject(hDC, hfOld);

    SetRect(&rc, 0, r->top, 60, r->bottom);
    char s_num[4];
    snprintf(s_num, sizeof(s_num), "#%02d", num);
    txtoutl(&rc, hDC, g_white, g_black, s_num, DT_CENTER);

    SetRect(&rc, 70, r->top, 200, r->top + 30);
    HBRUSH br = CreateSolidBrush(enabled ? g_green : g_red);
    FillRect(hDC, &rc, br);
    DeleteObject(br);
    char* status = enabled ? "ENABLED" : "DISABLED";
    txtoutl(&rc, hDC, g_white, g_black, status, DT_CENTER);
    r->top += 40;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HFONT hf;
    if (!hf) {
        hf = CreateFont(30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    }
    switch(message)
    {
    case WM_CHAR:
    {
        if(wParam==VK_ESCAPE)
            SendMessage(hwnd,WM_CLOSE,0,0);
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC;
        if (!visible) {
            hDC=BeginPaint(hwnd,&ps);
            EndPaint(hwnd,&ps);
            return 0;
        }
        COLORREF g_white = RGB(255, 255, 255);
        COLORREF g_black = RGB(1, 1, 1);
        hDC=BeginPaint(hwnd,&ps);
        HFONT hfOld = SelectObject(hDC, hf);
        SetBkMode(hDC, TRANSPARENT);
        RECT hwnd_rc;
        GetClientRect(hwnd, &hwnd_rc);
        txtoutl(&hwnd_rc, hDC, g_white, g_black, "RIOT CHEATS", DT_CENTER);
        hwnd_rc.top += 40;
        if (prev_cheats.invalid) {
            txtoutl(&hwnd_rc, hDC, g_white, g_black, "no valid data", DT_CENTER);
        } else {
            drawcheatstat(&hwnd_rc, hDC, 14, prev_cheats.c14, "RoughNeighbourhood");
            drawcheatstat(&hwnd_rc, hDC, 31, prev_cheats.c31, "AllDriversAreCriminals");
            drawcheatstat(&hwnd_rc, hDC, 69, prev_cheats.c69, "StateOfEmergency");
        }
        SelectObject(hDC, hfOld);
        EndPaint(hwnd,&ps);
        return 0;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc (hwnd, message, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hIns, HINSTANCE hPrev, LPSTR lpszArgument, int nCmdShow)
{
    char szClassName[]="GTA SA Riot Cheats";
    WNDCLASSEX wc;
    MSG messages;

    wc.hInstance=hIns;
    wc.lpszClassName=szClassName;
    wc.lpfnWndProc = WndProc;
    wc.style = CS_DBLCLKS;
    wc.cbSize = sizeof (WNDCLASSEX);
    wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.lpszMenuName = NULL;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground=CreateSolidBrush(RGB(0, 0, 0));
    RegisterClassEx(&wc);
    hwnd=CreateWindow(szClassName,szClassName,WS_OVERLAPPEDWINDOW,0,0,218,262,HWND_DESKTOP,NULL,hIns,NULL);
    ShowWindow(hwnd, nCmdShow);
    HANDLE thread = CreateThread(0, 0, dc_thread, NULL, 0, NULL);
    while(GetMessage(&messages, NULL, 0, 0))
    {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }
    CloseHandle(thread);
    return messages.wParam;
}
