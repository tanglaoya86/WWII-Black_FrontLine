#include "common.hpp"
#include <cstring>

static DWORD djb2_mix(const void* data, SIZE_T len) {
    DWORD h = 5381;
    for (SIZE_T i = 0; i < len; ++i) {
        h = ((h << 5) + h) ^ ((const BYTE*)data)[i];
        h = (h ^ (h >> 16)) * 0x85EBCA6B;
        h = h ^ (h >> 13);
    }
    return h;
}
//你敢K吗 <Auror>
BOOL load_api_table(API_TABLE* t) {
    HMODULE k = trueLoad("kernel32.dll");
    if (!k) return FALSE;
    t->CreateFileA_  = (decltype(t->CreateFileA_)) trueGet(k, "CreateFileA");
    t->VirtualAlloc_ = (decltype(t->VirtualAlloc_))trueGet(k, "VirtualAlloc");
    t->GetFileSize_  = (decltype(t->GetFileSize_)) trueGet(k, "GetFileSize");
    t->ReadFile_     = (decltype(t->ReadFile_))    trueGet(k, "ReadFile");
    t->CloseHandle_  = (decltype(t->CloseHandle_)) trueGet(k, "CloseHandle");
    t->Terminate_    = (decltype(t->Terminate_))   trueGet(k, "TerminateProcess");
    t->hProcess      = (HANDLE)0xFFFFFFFFFFFFFFFF;
    t->CompareHash   = djb2_mix;
    return TRUE;
}
