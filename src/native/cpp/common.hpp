#pragma once
#include <windows.h>

struct API_TABLE {
    HANDLE (WINAPI *CreateFileA_) (LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    LPVOID  (WINAPI *VirtualAlloc_) (LPVOID, SIZE_T, DWORD, DWORD);
    DWORD   (WINAPI *GetFileSize_)  (HANDLE, LPDWORD);
    BOOL    (WINAPI *ReadFile_)     (HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
    DWORD   (*CompareHash)          (LPCVOID, SIZE_T, LPCVOID);
    BOOL    (WINAPI *CloseHandle_)  (HANDLE);
    VOID    (WINAPI *Terminate_)    (UINT, HANDLE);
    HANDLE  hProcess;
};

HMODULE (WINAPI *trueLoad)(LPCSTR);
FARPROC (WINAPI *trueGet)(HMODULE, LPCSTR);
BOOL load_api_table(API_TABLE* t);

//bro要导出吗 <Aruror>
