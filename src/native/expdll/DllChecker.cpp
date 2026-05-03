// <Auror>
// 导出函数看这：
//   CheckDllsA (ANSI)  /  CheckDllsW (Unicode)
//   统一别名 CheckDlls → CheckDllsA  （保持旧兼容）
//   FreeResult：释放返回的 JSON 字符串
//其他注意事项:
//   JSON看最后
//   txt文件格式
//其实我打算把txt后续改为一个MD5的加盐哈希然后用 .bin 存储最后再核对但是...看你
//...
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <windows.h>
#include <shlwapi.h>        
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <memory>
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
static std::string wstr_to_u8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}
static std::string get_last_error_text(DWORD err) {
    LPWSTR buf = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buf, 0, nullptr);
    if (len > 0 && buf) {
        std::wstring wmsg(buf, len);
        LocalFree(buf);
        while (!wmsg.empty() && (wmsg.back() == L'\n' || wmsg.back() == L'\r'))
            wmsg.pop_back();
        return wstr_to_u8(wmsg);
    }
    return "Unknown error";
}
static std::string filetime_to_iso(FILETIME ft) {
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st))
        return "invalid time";
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
    return buf;
}
static int count_exports(HMODULE hModule) {
    if (!hModule) return -1;
    BYTE* base = reinterpret_cast<BYTE*>(hModule);
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return -1;
    IMAGE_NT_HEADERS64* nth = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        base + dos->e_lfanew);
    if (nth->Signature != IMAGE_NT_SIGNATURE)
        return -1;
    // 嘿看这里 我只处理 64 位 PE（若需 32 位请补充 Machine 判断好吗） <Auror>
    if (nth->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return -1;
    DWORD export_rva = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (export_rva == 0)
        return 0; 
    IMAGE_EXPORT_DIRECTORY* exp_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + export_rva);
    DWORD names = exp_dir->NumberOfNames;
    DWORD funcs = exp_dir->NumberOfFunctions;
    return (names > funcs ? names : funcs);
}
static std::string build_json_report(const std::wstring& config_path) {
    WCHAR dir[MAX_PATH] = {0};
    WCHAR file_name[MAX_PATH] = {0};
    wcscpy(dir, config_path.c_str());
    PathRemoveFileSpecW(dir);  
    wcscpy(file_name, config_path.c_str() + wcslen(dir));
    if (file_name[0] == L'\\' || file_name[0] == L'/')
        memmove(file_name, file_name + 1, (wcslen(file_name) + 1) * sizeof(WCHAR));
    if (PathIsDirectoryW(config_path.c_str())) {
        wcscpy(dir, config_path.c_str());
        wcscpy(file_name, L"nesdll.txt");
    }
    std::string config_file_u8 = wstr_to_u8(std::wstring(dir) + L"\\" + file_name);
    std::ifstream file(config_file_u8);
    if (!file.is_open()) {
        return "{\"error\":\"Cannot open config file\"}";
    }

    std::vector<std::string> dll_names;
    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (!trimmed.empty())
            dll_names.push_back(trimmed);
    }
    file.close();

    std::wstring dir_w = dir;

    std::ostringstream json;
    json << "{\"dlls\":[";

    for (size_t i = 0; i < dll_names.size(); ++i) {
        std::string name = trim(dll_names[i]);
        if (name.empty()) continue;
        std::string full_name = name;
        if (name.find('.') == std::string::npos)
            full_name += ".dll";
        std::wstring w_full_name(full_name.begin(), full_name.end());
        std::wstring full_path = dir_w + L"\\" + w_full_name;

        bool exists = false;
        bool loadable = false;
        int export_count = -1;
        std::string error;
        std::string file_size_str = "0";
        std::string mod_time_str = "";
        WIN32_FILE_ATTRIBUTE_DATA file_attr;
        if (GetFileAttributesExW(full_path.c_str(), GetFileExInfoStandard, &file_attr)) {
            exists = true;
            ULONGLONG file_size = ((ULONGLONG)file_attr.nFileSizeHigh << 32) | file_attr.nFileSizeLow;
            file_size_str = std::to_string(file_size);
            mod_time_str = filetime_to_iso(file_attr.ftLastWriteTime);
        }
        HMODULE h = LoadLibraryExW(full_path.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (h != NULL) {
            loadable = true;
            export_count = count_exports(h);
            if (export_count < 0) {
                error = "Failed to parse PE exports";
            } else if (export_count == 0) {
                error = "No export functions (might be a resource DLL)";
            }
            FreeLibrary(h);
        } else {
            DWORD err = GetLastError();
            loadable = false;
            error = get_last_error_text(err);
        }

        // 生成 JSON 对象 自己琢磨一下好吗  <Auror>
        json << "{\"name\":\"" << full_name << "\","
             << "\"exists\":" << (exists ? "true" : "false") << ","
             << "\"loadable\":" << (loadable ? "true" : "false") << ","
             << "\"export_count\":" << export_count << ","
             << "\"file_size\":" << file_size_str << ","
             << "\"modification_time\":\"" << (mod_time_str.empty() ? "N/A" : mod_time_str) << "\","
             << "\"error\":\"" << error << "\"}";

        if (i < dll_names.size() - 1) json << ",";
    }
    json << "]}";
    return json.str();
}
extern "C" __declspec(dllexport) const char* CheckDllsA(const char* path) {
    if (!path) return nullptr;
    int wlen = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    if (wlen <= 0) return nullptr;
    std::wstring wpath(wlen - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, path, -1, &wpath[0], wlen);

    std::string json = build_json_report(wpath);
    char* result = (char*)malloc(json.size() + 1);
    if (result) {
        memcpy(result, json.c_str(), json.size() + 1);
    }
    return result;
}

// Unicode 版本（推荐你的Python使用）  <Auror>
extern "C" __declspec(dllexport) const char* CheckDllsW(const wchar_t* path) {
    if (!path) return nullptr;
    std::string json = build_json_report(path);
    char* result = (char*)malloc(json.size() + 1);
    if (result) {
        memcpy(result, json.c_str(), json.size() + 1);
    }
    return result;
}
extern "C" __declspec(dllexport) const char* CheckDlls(const char* path) {
    return CheckDllsA(path);
}

extern "C" __declspec(dllexport) void FreeResult(const char* p) {
    free((void*)p);
}
BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}
/* 返回的JSON样例如下
{
  "dlls": [
    {
      "name": "kernel32.dll",
      "exists": true,
      "loadable": true,
      "export_count": 1632,
      "file_size": "802304",
      "modification_time": "2025-04-10T12:34:56Z",
      "error": ""
    },
    {
      "name": "broken.dll",
      "exists": true,
      "loadable": false,
      "export_count": -1,
      "file_size": "10240",
      "modification_time": "2025-01-01T00:00:00Z",
      "error": "The specified module could not be found"
    },
    {
      "name": "missing.dll",
      "exists": false,
      "loadable": false,
      "export_count": -1,
      "file_size": "0",
      "modification_time": "N/A",
      "error": "File not found"
    }
  ]
}
请你自己用Py处理吧*/
//注意了这里
/*
    nesdll.txt 的格式非常简单 一行一个 DLL 名称 支持以下特点：
    可带或不带.dll扩展名（不带会自动补全的）
    空行和首尾空格忽略
    不需要完整路径，只写文件名即可（程序会在指定目录下查找）
    不要包含路径分隔符或目录，否则会被当作文件名一部分然后肘击你的状态机
    
放置位置:
    默认会从传入的目录下读取 nesdll.txt  比如你调用 check_dlls(r"C:\MyAPP")，程序会去读C:\MyApp\nesdll.txt ，并在C:\MyApp\目录下寻找列出的 DLL
    
nesdll.txt示例如下
    
    kernel32
    user32.dll
    shell32
    ole32.dll
    等价于：
    kernel32.dll
    user32.dll
    shell32.dll
    ole32.dll
特别注意
    每一行只写一个 DLL 名称，不要写类似 C:\Windows\System32\kernel32.dll 这样的完整路径
    中文名等 Unicode 路径请通过 Python 的 CheckDllsW 传递，DLL 内部已处理
*/
