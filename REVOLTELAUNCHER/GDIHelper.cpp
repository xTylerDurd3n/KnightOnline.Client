
#include "stdafx.h"
//#include "Engine.h"
#include "GDIHelper.h"

extern GDIHelper gdiHelper;
extern HWND mainWindow;
HWND GDIHelper::staticControl = NULL;
HWND GDIHelper::MainHWNDControl = NULL;
Image* GDIHelper::m_pImage = NULL;
GUID* GDIHelper::m_pDimensionIDs = NULL;
UINT GDIHelper::m_FrameCount = 0;
PropertyItem* GDIHelper::m_pItem = NULL;
UINT GDIHelper::m_iCurrentFrame = 0;
UINT_PTR GDIHelper::unique_id = 0;
BOOL GDIHelper::m_bIsPlaying = FALSE;
BOOL GDIHelper::isPlayable = FALSE;
BOOL GDIHelper::isLooped = FALSE;
int GDIHelper::xPosition = 0;
int GDIHelper::yPosition = 0;
int GDIHelper::width = 0;
int GDIHelper::height = 0;
int GDIHelper::animation_duration = 0;

/** GDIHelper is a class helper to display images and animated GIF **/
GDIHelper::GDIHelper() {}

/** Function to destroy objects and arrays, call this function on WM_DESTROY of WinProc. **/
void GDIHelper::Destroy() {
    if(m_pDimensionIDs) {
        delete[] m_pDimensionIDs;
    }

    if(m_pItem) {
        free(m_pItem);
    }

    if(m_pImage) {
        delete m_pImage;
    }
    m_bIsPlaying = FALSE;
    isPlayable = FALSE;
    RemoveWindowSubclass(staticControl, &StaticControlProc, unique_id);
}

#include <iostream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

bool InjectDLL(DWORD pid, const char* dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess)
    {
        std::cerr << "OpenProcess failed: " << GetLastError() << std::endl;
        return false;
    }

    LPVOID pRemoteBuf = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemoteBuf)
    {
        std::cerr << "VirtualAllocEx failed: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuf, (LPVOID)dllPath, strlen(dllPath) + 1, NULL))
    {
        std::cerr << "WriteProcessMemory failed: " << GetLastError() << std::endl;
        VirtualAllocEx(hProcess, pRemoteBuf, 0, MEM_RELEASE, PAGE_READWRITE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
    if (!hKernel32)
    {
        std::cerr << "GetModuleHandle failed: " << GetLastError() << std::endl;
        VirtualAllocEx(hProcess, pRemoteBuf, 0, MEM_RELEASE, PAGE_READWRITE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibrary)
    {
        std::cerr << "GetProcAddress failed: " << GetLastError() << std::endl;
        VirtualAllocEx(hProcess, pRemoteBuf, 0, MEM_RELEASE, PAGE_READWRITE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteBuf, 0, NULL);
    if (!hThread)
    {
        std::cerr << "CreateRemoteThread failed: " << GetLastError() << std::endl;
        VirtualAllocEx(hProcess, pRemoteBuf, 0, MEM_RELEASE, PAGE_READWRITE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualAllocEx(hProcess, pRemoteBuf, 0, MEM_RELEASE, PAGE_READWRITE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}

std::string GetCurrentDirectory()
{
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    PathRemoveFileSpec(buffer);
    return std::string(buffer);
}

extern HWND hLoadHwnd;
//
//void NtResumeProcess(HANDLE hProcess)
//{
//    typedef LONG(NTAPI* NtResumeProcess)(HANDLE ProcessHandle);
//    NtResumeProcess pfnNtResumeProcess = (NtResumeProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtResumeProcess");
//    if (pfnNtResumeProcess)
//        pfnNtResumeProcess(hProcess);
//}

#include <iostream>
#include <sstream>
#include <Windows.h>
#include "Remap.h"

EXTERN_C NTSTATUS NTAPI NtSuspendProcess(HANDLE);
EXTERN_C NTSTATUS NTAPI NtResumeProcess(HANDLE);

BOOL StartProcess(std::string szFilePath, std::string szFile, std::string szCommandLine, PROCESS_INFORMATION& processInfo)
{
    STARTUPINFO info = { sizeof(info) };

    std::string szWorkingDirectory(szFilePath.begin(), szFilePath.end());
    std::string strFileName(szFile.begin(), szFile.end());

    std::ostringstream fileArgs;
    fileArgs << szWorkingDirectory.c_str() << "\\" << strFileName.c_str();

    std::string file = fileArgs.str();

    std::ostringstream cmdArgs;
    cmdArgs << "\\" << szWorkingDirectory.c_str() << "\\" << strFileName.c_str() << "\\";
    cmdArgs << " ";
    cmdArgs << szCommandLine.c_str();

    std::string cmd = cmdArgs.str();

    SECURITY_ATTRIBUTES securityInfo = { sizeof(securityInfo) };

    securityInfo.bInheritHandle = FALSE;

    BOOL result = CreateProcessA(&file[0], &cmd[0], &securityInfo, NULL, FALSE, 0, NULL, &szWorkingDirectory[0], &info, &processInfo);

    if (!result)
        return FALSE;

    return TRUE;
}

inline static DWORD Read4Byte(HANDLE hProcess, DWORD dwAddress)
{
    DWORD nValue = 0;
    ReadProcessMemory(hProcess, (LPVOID)dwAddress, &nValue, 4, 0);
    return nValue;
}

// Thread olarak �al��acak fonksiyon
void threadFunction(HANDLE hProcess) {
    while (Read4Byte(hProcess, 0x0131AA1B) == 0)
        continue;
    BYTE byPatchs[] = { 0xC3, 0x90 };
    WriteProcessMemory(hProcess, (LPVOID*)0x0131AA1B, byPatchs, sizeof(byPatchs), 0);
}

// Function to get the handle of the main window of a process
HWND GetMainWindowHandle(DWORD processId) {
    HWND hwnd = GetTopWindow(NULL);
    while (hwnd != NULL) {
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId == processId) {
            return hwnd;
        }
        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return NULL;
}

// Thread i�levi
DWORD WINAPI ThreadFunc(LPVOID lpParam) {
    while (true) {
        // ��lem kolu alma
        HANDLE hProcess = reinterpret_cast<HANDLE>(lpParam);

        // ��lem kolu ge�erli mi kontrol�
        if (hProcess == NULL || hProcess == INVALID_HANDLE_VALUE) {
            std::cerr << "Ge�ersiz i�lem kolu\n";
            return 1;
        }

        // ��lem koluyla ilgili i�lemleri ger�ekle�tirin...
        std::cout << "Thread i�lem koluyla �al���yor\n";
        BYTE byPatchs[] = { 0xC3, 0x90 };
        WriteProcessMemory(hProcess, (LPVOID*)0x0131AA1B, byPatchs, sizeof(byPatchs), 0);
        // 10 saniye bekleme
        Sleep(1000); // 10 saniye = 10000 milisaniye
    }

    return 0;
    //// ��lem kolu alma
    //HANDLE hProcess = reinterpret_cast<HANDLE>(lpParam);

    //// ��lem kolu ge�erli mi kontrol�
    //if (hProcess == NULL || hProcess == INVALID_HANDLE_VALUE) {
    //    std::cerr << "Ge�ersiz i�lem kolu\n";
    //    return 1;
    //}

    //while (Read4Byte(hProcess, 0x0131AA1B) == 0)
    //    continue;

    //BYTE byPatchs[] = { 0xC3, 0x90 };
    //WriteProcessMemory(hProcess, (LPVOID*)0x0131AA1B, byPatchs, sizeof(byPatchs), 0);

    //return 0;
}

/** Private function, call this function as thread to animate the GIF image. **/
void GDIHelper::run()
{
    if(m_bIsPlaying == TRUE) {
        return;
    }
    m_iCurrentFrame = 0;
    GUID Guid = FrameDimensionTime;
    m_pImage->SelectActiveFrame(&Guid, m_iCurrentFrame);
    ++m_iCurrentFrame;
    m_bIsPlaying = TRUE;
    animation_duration = ((UINT*)m_pItem[0].value)[m_iCurrentFrame] * 10;
    
    while(isPlayable) {
        std::this_thread::sleep_for(std::chrono::milliseconds(animation_duration));
        m_pImage->SelectActiveFrame(&Guid, m_iCurrentFrame);

        m_iCurrentFrame = (++m_iCurrentFrame) % m_FrameCount;
        InvalidateRect(staticControl, NULL, FALSE);
        UpdateWindow(staticControl);
       
        if (!isLooped && m_iCurrentFrame == 0)
        {
            ShowWindow(hLoadHwnd, SW_HIDE);
            ShowWindow(staticControl, SW_HIDE);
            gdiHelper.Destroy();
#if 0
            PROCESS_INFORMATION clientProcessInfo;

            std::ostringstream szCommandLine;

            szCommandLine << GetCurrentProcessId();

            //std::string dllPath = GetCurrentDirectory() + "\\hsacsx.dll";

            if (!StartProcess(GetCurrentDirectory(), "KnightOnLine.exe", szCommandLine.str(), clientProcessInfo))
            {
                MessageBoxA(mainWindow, "KnightOnLine.exe not found.", "Launcher", MB_ICONINFORMATION);
                ::PostQuitMessage(0);
                TerminateProcess(GetCurrentProcess(), 0);
                ExitProcess(0);
            }

            /*if (!InjectDLL(clientProcessInfo.dwProcessId, dllPath.c_str()))
            {
                MessageBoxA(mainWindow, xorstr("hsacsx.dll not found."), xorstr("Launcher"), MB_ICONINFORMATION);
                TerminateProcess(clientProcessInfo.hProcess, 0);
                ::PostQuitMessage(0);
                TerminateProcess(GetCurrentProcess(), 0);
                ExitProcess(0);
                return;
            }*/       

            while (Read4Byte(clientProcessInfo.hProcess, 0x00E706F7) == 0)
                continue;

            NtSuspendProcess(clientProcessInfo.hProcess);

            Remap::PatchSection(clientProcessInfo.hProcess, (LPVOID*)0x00400000, 0x00B20000, PAGE_EXECUTE_READWRITE);
            Remap::PatchSection(clientProcessInfo.hProcess, (LPVOID*)0x00F20000, 0x00010000, PAGE_EXECUTE_READWRITE);
            //Remap::PatchSection(clientProcessInfo.hProcess, (LPVOID*)0x00F30000, 0x00130000, PAGE_EXECUTE_READWRITE);
            Remap::PatchSection(clientProcessInfo.hProcess, (LPVOID*)0x012F0000, 0x007C0000, PAGE_EXECUTE_READWRITE);

            // Hedef bellekte yazmak istedi�iniz adresi belirtin (bu �rnekte varsay�lan bir adres kullan�lm��t�r)
            LPVOID baseAddress = (LPVOID)0x00E706F7; // De�i�tirin: hedef bellek adresi
            BYTE byPatch[] = { 0xE9, 0x79, 0x06, 0x00, 0x00, 0x90 }; // Yazmak istedi�iniz veri
            WriteProcessMemory(clientProcessInfo.hProcess, baseAddress, &byPatch, sizeof(byPatch), 0);

            NtResumeProcess(clientProcessInfo.hProcess);

            while (Read4Byte(clientProcessInfo.hProcess, 0x0131AA22) == 0)
                continue;

            NtSuspendProcess(clientProcessInfo.hProcess);

            BYTE byPatchs[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
            WriteProcessMemory(clientProcessInfo.hProcess, (LPVOID*)0x0131AA22, byPatchs, sizeof(byPatchs), 0);

            NtResumeProcess(clientProcessInfo.hProcess);

            HWND hwnd = GetMainWindowHandle(clientProcessInfo.dwProcessId);
            if (hwnd == NULL) {
                printf("Failed to find the main window handle.\n");
                return;
            }

            // New window title
            TCHAR newTitle[21];
            _stprintf_s(newTitle, _T("REVOLTEACS Client[%d]"), clientProcessInfo.dwProcessId);

            // Set the new window title
            if (!SetWindowText(hwnd, newTitle)) {
                printf("Failed to set the window title. Error: %d\n", GetLastError());
                return;
            }

            printf("Successfully changed the window title to: %s\n", newTitle);
            CloseHandle(clientProcessInfo.hProcess);
#endif
#if 1
            std::string currentDir = GetCurrentDirectory();
            std::string dllPath = currentDir + "\\REVOLTEACS.dll";

            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            /*if (!CreateProcessA(NULL, &commandLine[0], NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {*/
            std::string executablePath = "KnightOnline.exe";
            std::string parameters = "E03ED890-8E94-4B42-B1C5-3CDA401AA9C2";
            std::string commandLine = executablePath + " " + parameters;

            char fullPath[MAX_PATH];
            GetModuleFileNameA(NULL, fullPath, MAX_PATH);
            std::string::size_type pos = std::string(fullPath).find_last_of("\\/");
            std::string startupPath = std::string(fullPath).substr(0, pos + 1) + executablePath;

            // CreateProcessA �a�r�s�
            if (!CreateProcessA(NULL, const_cast<LPSTR>(commandLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            {
                MessageBoxA(mainWindow, "KnightOnLine.exe not found.", "Launcher", MB_ICONINFORMATION);
                goto return_true;
            }

            std::cout << "Process created and suspended. Injecting DLL..." << std::endl;

            if (!InjectDLL(pi.dwProcessId, dllPath.c_str()))
            {
                MessageBoxA(mainWindow, xorstr("REVOLTEACS.dll not found."), xorstr("Launcher"), MB_ICONINFORMATION);
                TerminateProcess(pi.hProcess, 0);
                goto return_true;
            }


            std::cout << "DLL injected. Waiting for 2 seconds..." << std::endl;
            Sleep(2000);

            std::cout << "Resuming process..." << std::endl;
            ResumeThread(pi.hThread);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        return_true:
#endif
            ::PostQuitMessage(0);
            TerminateProcess(GetCurrentProcess(), 0);
            ExitProcess(0);
            break;
        }
    }
}

/** Private function, accessible only in this class, check if file exist. **/
bool GDIHelper::IsFileExist(string file_name) {
    struct stat buffer;
    return (stat(file_name.c_str(), &buffer) == 0);
}

/** Private function, function to count and get the frame of image. **/
void GDIHelper::GetImageFrame() {
    UINT count = m_pImage->GetFrameDimensionsCount();
    m_pDimensionIDs = new GUID[count];
    m_pImage->GetFrameDimensionsList(m_pDimensionIDs, count);

    m_FrameCount = m_pImage->GetFrameCount(&m_pDimensionIDs[0]);

    UINT TotalBuffer = m_pImage->GetPropertyItemSize(PropertyTagFrameDelay);
    m_pItem = (PropertyItem*)malloc(TotalBuffer);
    m_pImage->GetPropertyItem(PropertyTagFrameDelay, TotalBuffer, m_pItem);

    if(m_FrameCount > 1) {  // frame of GIF is more than one, all good, we don't want the error of `Access violation reading location`
        OutputDebugString(_T("NOTICED: GDIHelper::InitializeImage >> Image file has more than 1 frame, its playable (2).\n"));
        isPlayable = TRUE;  // is playable
    
        std::thread t(run); // Start the animation as thread.
        t.detach();         // this will be non-blocking thread.
    }
}

LRESULT CALLBACK GDIHelper::StaticControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg)
    {
    case WM_PAINT: 
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        Graphics g(hdc);
        g.DrawImage(m_pImage, 0, 0, width, height);

        EndPaint(hwnd, &ps);
        return TRUE;
    }
    break;
    case WM_LBUTTONDBLCLK:
    {
        int a = 0;
    }
    break;
    }
   return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

/** Function to set the image and initialize all required variables. **/
void GDIHelper::SetImage(int uunique_id, int xxPosition, int yyPosition, int wwidth, int hheight, Image* image, HWND hwnd) {
    unique_id = uunique_id;
    xPosition = xxPosition;
    yPosition = yyPosition;
    width = wwidth;
    height = hheight;

    staticControl = CreateWindowEx(0, "STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, xPosition, yPosition, width, height, hwnd, NULL, NULL, NULL); //create the static control.
    SetWindowSubclass(staticControl, &StaticControlProc, unique_id, 0);

    m_pImage = image;
    GetImageFrame(); //Initialize the image.
}

/** Function to Display Image from Local File. **/
void GDIHelper::DisplayImageFromFile(string file_name, HWND hWnd, UINT_PTR uunique_id, int xxPosition, int yyPosition, int wwidth, int hheight, bool looped) {
    if(!IsFileExist(file_name)) {
        OutputDebugString(_T("ERROR: GDIHelper::LoadImageFromFile >> Invalid file or not exist\n"));
        return; 
    }

    isLooped = looped;
    std::wstring widestr = std::wstring(file_name.begin(), file_name.end()); // Convert the string file_name to wstring.
    SetImage(uunique_id, xxPosition, yyPosition, wwidth, hheight, Image::FromFile(widestr.c_str()), hWnd); //Set image and Control
}


/** Function to Display Image from Local File. **/
void GDIHelper::thyke_display(string file_name, HWND hWndKO, HWND &hWnd, UINT_PTR uunique_id, int xxPosition, int yyPosition, int wwidth, int hheight, bool looped) {
    if(!IsFileExist(file_name)) {
        OutputDebugString(_T("ERROR: GDIHelper::LoadImageFromFile >> Invalid file or not exist\n"));
        return; 
    }

    isLooped = looped;
    std::wstring widestr = std::wstring(file_name.begin(), file_name.end()); // Convert the string file_name to wstring.
   
    unique_id = uunique_id;
    xPosition = xxPosition;
    yPosition = yyPosition;
    width = wwidth;
    height = hheight;

    staticControl = CreateWindowEx(0, "STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, xPosition, yPosition, width, height, hWndKO, NULL, NULL, NULL); //create the static control.
    hWnd = staticControl;
    SetWindowSubclass(staticControl, &StaticControlProc, unique_id, 0);

    m_pImage = Image::FromFile(widestr.c_str());
    GetImageFrame(); //Initialize the image.
}

/** Function to Load Image from Resources. **/
void GDIHelper::DisplayImageFromResource(HMODULE hMod, const wchar_t* resid, const wchar_t* restype, HWND hWnd, UINT_PTR uunique_id, int xxPosition, int yyPosition, int wwidth, int hheight) {

    IStream* pStream = nullptr;
    Gdiplus::Bitmap* pBmp = nullptr;
    HGLOBAL hGlobal = nullptr;

    HRSRC hrsrc = FindResourceW(GetModuleHandle(NULL), resid, restype);     // get the handle to the resource
    if(hrsrc) {
        DWORD dwResourceSize = SizeofResource(hMod, hrsrc);
        if(dwResourceSize > 0) {
            HGLOBAL hGlobalResource = LoadResource(hMod, hrsrc); // load it
            if(hGlobalResource) {
                void* imagebytes = LockResource(hGlobalResource); // get a pointer to the file bytes

                hGlobal = ::GlobalAlloc(GHND, dwResourceSize); // copy image bytes into a real hglobal memory handle
                if(hGlobal) {
                    void* pBuffer = ::GlobalLock(hGlobal);
                    if(pBuffer) {
                        memcpy(pBuffer, imagebytes, dwResourceSize);
                        HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pStream);
                        if(SUCCEEDED(hr)) {
                            hGlobal = nullptr; // pStream now owns the global handle and will invoke GlobalFree on release
                            pBmp = new Gdiplus::Bitmap(pStream);
                        }
                    }
                }
            }
        }
    }

    if(pStream) {
        pStream->Release();
        pStream = nullptr;
    }

    if(hGlobal) {
        GlobalFree(hGlobal);
    }

    SetImage(uunique_id, xxPosition, yyPosition, wwidth, hheight, pBmp, hWnd);
}
