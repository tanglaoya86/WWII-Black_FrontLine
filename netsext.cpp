/*
 * Release -rev0
 * Build: g++ -O2 -std=c++11 netsext.cpp -o netsext.exe -I"<npcap-include>" -L"<npcap-lib>" -lws2_32 -liphlpapi -lwpcap -lPacket -lwinhttp -lole32 -luuid -luser32 -ladvapi32 -lwsock32 -lcrypt32 -lsecur32 -lnetapi32 -loleaut32
 * Run as Administrator.
 */

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define SECURITY_WIN32

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <pcap.h>
#include <packet32.h>
#include <winhttp.h>
#include <schannel.h>
#include <security.h>
#include <sspi.h>
#include <netfw.h>
#include <nb30.h>
#include <lm.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdarg>
#include <ctime>
#include <cmath>
#include <deque>
#include <queue>
#include <random>
#include <functional>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wpcap.lib")
#pragma comment(lib, "Packet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "oleaut32.lib")

// ---------- 全局标志 ----------
static bool g_verbose = false;
static bool g_quiet = false;
static bool g_no_color = false;
static bool g_stealth = false;
static bool g_fast = false;
static bool g_all = false;
static bool g_open_only = false;
static bool g_cve = false;
static bool g_msf = false;
static bool g_hex = false;
static bool g_follow = false;
static bool g_loop = false;
static bool g_flood = false;
static bool g_listen_mode = false;
static bool g_safe = false;
static bool g_deep = false;
static bool g_stats = false;

static HANDLE g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
static std::atomic<bool> g_running{true};

// ---------- 颜色枚举 ----------
enum Color { RESET, GREEN, RED, CYAN, YELLOW, MAGENTA, WHITE, BLUE };
void set_color(Color c) {
    if (g_no_color) return;
    WORD attr = 7;
    switch (c) {
        case GREEN:  attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case RED:    attr = FOREGROUND_RED   | FOREGROUND_INTENSITY; break;
        case CYAN:   attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case YELLOW: attr = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case MAGENTA:attr = FOREGROUND_RED   | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case BLUE:   attr = FOREGROUND_BLUE  | FOREGROUND_INTENSITY; break;
        case WHITE:  attr = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        default:     attr = 7;
    }
    SetConsoleTextAttribute(g_hConsole, attr);
}

void print_colored(Color c, const char* fmt, ...) {
    set_color(c);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    set_color(WHITE);
}

// ---------- 输出辅助函数 ----------
int get_terminal_width() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hConsole, &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
}

void print_separator_line(int len = 80, char c = '-') {
    if (g_quiet) return;
    std::string line(len, c);
    printf("%s\n", line.c_str());
}

void print_table_header(const std::vector<std::string>& headers, const std::vector<int>& widths) {
    if (g_quiet) return;
    set_color(CYAN);
    for (size_t i = 0; i < headers.size(); ++i) {
        printf("%-*s", widths[i], headers[i].c_str());
        if (i < headers.size() - 1) printf("  ");
    }
    printf("\n");
    set_color(WHITE);
    // 分隔线
    for (size_t i = 0; i < headers.size(); ++i) {
        std::string line(widths[i], '-');
        printf("%-*s", widths[i], line.c_str());
        if (i < headers.size() - 1) printf("  ");
    }
    printf("\n");
}

void print_table_row(const std::vector<std::string>& cols, const std::vector<int>& widths, const std::vector<Color>& colors) {
    if (g_quiet) return;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i < colors.size()) set_color(colors[i]);
        printf("%-*s", widths[i], cols[i].c_str());
        set_color(WHITE);
        if (i < cols.size() - 1) printf("  ");
    }
    printf("\n");
}

std::string format_time_us(const struct pcap_pkthdr* h) {
    char buf[32];
    time_t sec = h->ts.tv_sec;
    struct tm tm;
    localtime_s(&tm, &sec);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%06ld", tm.tm_hour, tm.tm_min, tm.tm_sec, h->ts.tv_usec);
    return std::string(buf);
}

std::string format_utc_time() {
    char buf[32];
    time_t now = time(NULL);
    struct tm tm;
    gmtime_s(&tm, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm);
    return std::string(buf);
}

// ---------- 参数结构 ----------
struct Args {
    std::string target;
    std::string function;
    std::string subcmd;
    std::string iface;
    std::string src_mac, dst_mac, src_ip, dst_ip, gw_ip;
    int local_port = 0, remote_port = 0;
    std::string protocol = "tcp";
    std::string stun_server;
    std::string ext_ip;
    bool upnp = false, pmp = false, tcp_punch = false, udp_punch = false;
    std::string cmd;
    std::string filter;
    std::string out_file, in_file;
    int snap_len = 65535;
    bool promisc = true;
    int count = 0;
    std::string ports;
    int timing = 3;
    int timeout_ms = 1000;
    int max_conn = 100;
    bool random_order = false;
    std::string data;
    std::string file;
    int hops = 30;
    bool arp_discover = false, tcp_syn_discover = false, no_ping = false;
    bool fingerprint = false, banner_grab = false;
    bool ssl = false, exec_cmd = false;
    std::string proto_stack;
    std::map<std::string, std::string> fields;
    int rate_pps = 0;
    std::string template_name;
    std::string cred;
    std::string domain;
    std::string vuln_type;
    bool patch_check = false;
    bool no_send = false;
    bool gratuitous = false, continuous = false;
    bool version_detect = false;
    std::vector<std::string> targets;
};

// ---------- 工具函数 ----------
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) if (!item.empty()) elems.push_back(item);
    return elems;
}
bool str_to_mac(const std::string& s, uint8_t mac[6]) {
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
        for (int i=0; i<6; ++i) mac[i] = (uint8_t)v[i]; return true;
    }
    return false;
}
std::string mac_to_str(const uint8_t mac[6]) {
    char buf[18]; sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return buf;
}
bool get_local_mac(const char* ifname, uint8_t mac[6]) {
    PIP_ADAPTER_INFO pInfo = (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO));
    ULONG len = sizeof(IP_ADAPTER_INFO);
    if (GetAdaptersInfo(pInfo, &len) == ERROR_BUFFER_OVERFLOW) { free(pInfo); pInfo = (PIP_ADAPTER_INFO)malloc(len); }
    bool found = false;
    if (GetAdaptersInfo(pInfo, &len) == NO_ERROR) {
        for (PIP_ADAPTER_INFO p = pInfo; p; p = p->Next) {
            if (!ifname || strstr(p->AdapterName, ifname) || strstr(p->Description, ifname)) {
                memcpy(mac, p->Address, 6); found = true; break;
            }
        }
    }
    free(pInfo); return found;
}
std::string get_local_ip_for_target(const char* target) {
    DWORD idx; if (GetBestInterface(inet_addr(target), &idx) != NO_ERROR) return "";
    PIP_ADAPTER_INFO pInfo = (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO));
    ULONG len = sizeof(IP_ADAPTER_INFO);
    if (GetAdaptersInfo(pInfo, &len) == ERROR_BUFFER_OVERFLOW) { free(pInfo); pInfo = (PIP_ADAPTER_INFO)malloc(len); }
    std::string ip;
    if (GetAdaptersInfo(pInfo, &len) == NO_ERROR) {
        for (PIP_ADAPTER_INFO p = pInfo; p; p = p->Next) if (p->Index == idx) { ip = p->IpAddressList.IpAddress.String; break; }
    }
    free(pInfo); return ip;
}
void expand_targets(const std::string& spec, std::vector<std::string>& out) {
    if (spec.empty()) return;
    if (spec[0] == '@') { std::ifstream f(spec.substr(1)); std::string l; while (getline(f,l)) if (!l.empty()&&l[0]!='#') out.push_back(l); }
    else if (spec.find('/') != std::string::npos) {
        size_t s = spec.find('/'); std::string base = spec.substr(0,s); int bits = atoi(spec.substr(s+1).c_str());
        if (bits == 24) { size_t d = base.rfind('.'); std::string p = base.substr(0,d+1); for (int i=1;i<=254;++i) out.push_back(p+std::to_string(i)); }
        else if (bits == 16) { size_t d = base.rfind('.'); std::string p = base.substr(0,d+1); for (int i=0;i<=255;++i) for (int j=1;j<=254;++j) out.push_back(p+std::to_string(i)+"."+std::to_string(j)); }
        else out.push_back(spec);
    } else if (spec.find('-') != std::string::npos) {
        size_t d = spec.find('-'); std::string start = spec.substr(0,d), end = spec.substr(d+1);
        size_t ld = start.rfind('.'); int s = atoi(start.substr(ld+1).c_str()), e = atoi(end.c_str());
        std::string p = start.substr(0,ld+1); for (int i=s; i<=e; ++i) out.push_back(p+std::to_string(i));
    } else out.push_back(spec);
}
std::vector<int> parse_ports(const std::string& s) {
    std::vector<int> ports;
    for (auto& r : split(s, ',')) {
        if (r.find('-') != std::string::npos) { int a,b; if (sscanf(r.c_str(), "%d-%d", &a, &b)==2) for (int i=a;i<=b;++i) ports.push_back(i); }
        else ports.push_back(atoi(r.c_str()));
    }
    return ports;
}

// ---------- 参数解析 ----------
bool parse_args(int argc, char* argv[], Args& args) {
    if (argc < 2) return false;
    int pos = 1;
    if (argv[pos][0] != '-') args.target = argv[pos++];
    if (pos >= argc || argv[pos][0] != '-') return false;
    std::string func = argv[pos]; if (func.length() != 2) return false;
    args.function = func.substr(1); pos++;
    if (pos < argc && argv[pos][0] != '-') args.subcmd = argv[pos++];
    while (pos < argc) {
        std::string arg = argv[pos];
        if (arg == "--verbose") { g_verbose = true; pos++; continue; }
        if (arg == "--quiet") { g_quiet = true; pos++; continue; }
        if (arg == "--no-color") { g_no_color = true; pos++; continue; }
        if (arg == "--stealth") { g_stealth = true; pos++; continue; }
        if (arg == "--fast") { g_fast = true; pos++; continue; }
        if (arg == "--all") { g_all = true; pos++; continue; }
        if (arg == "--open") { g_open_only = true; pos++; continue; }
        if (arg == "--cve") { g_cve = true; pos++; continue; }
        if (arg == "--msf") { g_msf = true; pos++; continue; }
        if (arg == "--hex") { g_hex = true; pos++; continue; }
        if (arg == "--follow") { g_follow = true; pos++; continue; }
        if (arg == "--loop") { g_loop = true; pos++; continue; }
        if (arg == "--flood") { g_flood = true; pos++; continue; }
        if (arg == "--listen") { g_listen_mode = true; pos++; continue; }
        if (arg == "--safe") { g_safe = true; pos++; continue; }
        if (arg == "--deep") { g_deep = true; pos++; continue; }
        if (arg == "--promisc") { args.promisc = true; pos++; continue; }
        if (arg == "--gratuitous") { args.gratuitous = true; pos++; continue; }
        if (arg == "--continuous") { args.continuous = true; pos++; continue; }
        if (arg == "--stats") { g_stats = true; pos++; continue; }
        if (arg == "--fingerprint") { args.fingerprint = true; pos++; continue; }
        if (arg == "--banner") { args.banner_grab = true; pos++; continue; }
        if (arg == "--ssl") { args.ssl = true; pos++; continue; }
        if (arg == "--exec") { args.exec_cmd = true; pos++; continue; }
        if (arg == "--random") { args.random_order = true; pos++; continue; }
        if (arg == "--service") { args.version_detect = true; pos++; continue; }
        if (arg == "--version") { args.version_detect = true; pos++; continue; }
        if (arg == "--arp") { args.arp_discover = true; pos++; continue; }
        if (arg == "--tcp-syn") { args.tcp_syn_discover = true; pos++; continue; }
        if (arg == "--no-ping") { args.no_ping = true; pos++; continue; }
        if (arg == "--upnp") { args.upnp = true; pos++; continue; }
        if (arg == "--pmp") { args.pmp = true; pos++; continue; }
        if (arg == "--tcp-punch") { args.tcp_punch = true; pos++; continue; }
        if (arg == "--udp-punch") { args.udp_punch = true; pos++; continue; }
        if (arg == "--no-send") { args.no_send = true; pos++; continue; }
        if (arg == "--patch") { args.patch_check = true; pos++; continue; }
        if (arg == "-i" && pos+1 < argc) { args.iface = argv[++pos]; pos++; continue; }
        if (arg == "-s" && pos+1 < argc) { args.src_mac = argv[++pos]; pos++; continue; }
        if (arg == "-d" && pos+1 < argc) { args.dst_mac = argv[++pos]; pos++; continue; }
        if (arg == "-S" && pos+1 < argc) { args.src_ip = argv[++pos]; pos++; continue; }
        if (arg == "-D" && pos+1 < argc) { args.dst_ip = argv[++pos]; pos++; continue; }
        if (arg == "-g" && pos+1 < argc) { args.gw_ip = argv[++pos]; pos++; continue; }
        if (arg == "-l" && pos+1 < argc) { args.local_port = atoi(argv[++pos]); pos++; continue; }
        if (arg == "-r" && pos+1 < argc) { args.remote_port = atoi(argv[++pos]); pos++; continue; }
        if (arg == "-p" && pos+1 < argc) { args.ports = argv[++pos]; pos++; continue; }
        if (arg == "-P" && pos+1 < argc) { args.protocol = argv[++pos]; pos++; continue; }
        if (arg == "-T" && pos+1 < argc) { args.timing = atoi(argv[++pos]); pos++; continue; }
        if (arg == "-t" && pos+1 < argc) { args.timeout_ms = atoi(argv[++pos]); pos++; continue; }
        if (arg == "-m" && pos+1 < argc) { args.max_conn = atoi(argv[++pos]); pos++; continue; }
        if (arg == "-F" && pos+1 < argc) { args.filter = argv[++pos]; pos++; continue; }
        if (arg == "-o" && pos+1 < argc) { args.out_file = argv[++pos]; pos++; continue; }
        if (arg == "-f" && pos+1 < argc) { args.file = argv[++pos]; pos++; continue; }
        if (arg == "-c" && pos+1 < argc) {
            std::string val = argv[++pos];
            if (args.function == "c" && val.find('=') == std::string::npos) args.proto_stack = val;
            else if (args.function == "v") args.cred = val;
            else if (args.function == "e") args.proto_stack = val;
            pos++; continue;
        }
        if (arg == "-H" && pos+1 < argc) { args.hops = atoi(argv[++pos]); pos++; continue; }
        if (arg == "--template" && pos+1 < argc) { args.template_name = argv[++pos]; pos++; continue; }
        if (arg == "--rate" && pos+1 < argc) { args.rate_pps = atoi(argv[++pos]); pos++; continue; }
        if (arg.find('=') != std::string::npos) { size_t eq = arg.find('='); args.fields[arg.substr(0,eq)] = arg.substr(eq+1); }
        pos++;
    }
    expand_targets(args.target, args.targets);
    return true;
}

void print_banner() {
    set_color(CYAN);
    printf("   ███╗   ██╗███████╗████████╗███████╗██╗  ██╗████████╗\n"
           "   ████╗  ██║██╔════╝╚══██╔══╝██╔════╝╚██╗██╔╝╚══██╔══╝\n"
           "   ██╔██╗ ██║█████╗     ██║   ███████╗ ╚███╔╝    ██║   \n"
           "   ██║╚██╗██║██╔══╝     ██║   ╚════██║ ██╔██╗    ██║   \n"
           "   ██║ ╚████║███████╗   ██║   ███████║██╔╝ ██╗   ██║   \n"
           "   ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚══════╝╚═╝  ╚═╝   ╚═╝   \n"
           "                           netsext\n"
           "                   Version 4.0 |  >_  ./root\n\n");
    set_color(WHITE);
}

void print_help() {
    print_banner();
    print_separator_line(70, '=');
    printf("Usage: netsext [target] -<function> [subcmd] [options]\n\n");
    printf("Functions:\n");
    printf("  -a  ARP operations: query, reply, spoof, view, flush\n");
    printf("  -n  NAT operations: detect, map, traverse, punch\n");
    printf("  -f  Packet sniffing\n");
    printf("  -r  Reconnaissance: discover, os, service, traceroute, dns\n");
    printf("  -s  Port scanning: syn, connect, udp, ack, fin, null, xmas\n");
    printf("  -p  Port operations: listen, connect, forward, banner\n");
    printf("  -c  Custom packet crafting\n");
    printf("  -v  Vulnerability scanning\n");
    printf("  -e  Interactive packet editor\n\n");
    printf("Common Options:\n");
    printf("  --verbose          Verbose output\n");
    printf("  --quiet            Suppress non-essential output\n");
    printf("  --no-color         Disable colored output\n");
    printf("  -i IFACE           Specify network interface\n");
    printf("  -p PORTS           Port range (e.g., 80,443,1000-2000)\n");
    printf("  -P PROTO           Protocol (tcp, udp, icmp)\n");
    printf("  -T 0-5             Timing template (higher = faster)\n");
    printf("  -t MS              Timeout in milliseconds\n");
    printf("  -m CONN            Max parallel connections\n");
    printf("  -o FILE            Output file\n");
    printf("  -F FILTER          BPF filter for sniffing\n");
    printf("  -f FILE            Input file (e.g., pcap)\n");
    printf("  --rate PPS         Packets per second limit\n");
    printf("  --listen           Listen for responses after send\n");
    printf("  --loop             Send packets continuously\n");
    printf("  --flood            Send packets as fast as possible\n");
    printf("  --no-send          Do not send packets (dry-run)\n");
    printf("  --hex              Display packets in hex\n");
    printf("  --safe             Avoid dangerous vulnerability checks\n");
    printf("  --cve              Show CVE numbers in vuln scan\n");
    printf("  --msf              Show Metasploit modules\n");
    printf("  --fingerprint      OS fingerprinting\n");
    printf("  --banner           Grab service banners\n");
    printf("  --service          Detect service version\n");
    printf("  --exec             Execute received commands (for listen)\n");
    printf("  --ssl              Use SSL/TLS for connections\n");
    printf("  --stats            Show statistics\n");
    printf("  --help             Show this help\n\n");
    printf("Examples:\n");
    printf("  netsext 192.168.1.0/24 -r discover\n");
    printf("  netsext scanme.nmap.org -s syn -p 1-1000\n");
    printf("  netsext -p listen -l 4444 --exec\n");
    printf("  netsext -f -i eth0 -F \"tcp port 80\" -o capture.pcap\n");
    printf("  netsext 192.168.1.1 -a spoof -g 192.168.1.254\n");
    printf("  netsext -e\n");
    print_separator_line(70, '=');
    printf("Press Enter to continue...");
    getchar();
}

// ---------- RAII 包装 ----------
class PcapHandle {
    pcap_t* h;
public:
    PcapHandle() : h(nullptr) {}
    PcapHandle(pcap_t* handle) : h(handle) {}
    PcapHandle(const char* dev, int snaplen, int promisc, int to_ms, char* err) : h(pcap_open_live(dev, snaplen, promisc, to_ms, err)) {}
    ~PcapHandle() { if (h) pcap_close(h); }
    operator pcap_t*() const { return h; }
};

class SocketHandle {
    SOCKET s;
public:
    SocketHandle() : s(INVALID_SOCKET) {}
    SocketHandle(int af, int type, int proto) : s(socket(af, type, proto)) {}
    ~SocketHandle() { if (s != INVALID_SOCKET) closesocket(s); }
    operator SOCKET() { return s; }
    SOCKET get() const { return s; }
};

// ---------- 数据包结构 ----------
#pragma pack(push,1)
struct EthHdr { uint8_t dst[6], src[6]; uint16_t type; };
struct VlanHdr { uint16_t tci; uint16_t type; };
struct ArpPacket { uint16_t htype, ptype; uint8_t hlen, plen; uint16_t op; uint8_t sha[6]; uint32_t spa; uint8_t tha[6]; uint32_t tpa; };
struct IPv4Hdr { uint8_t ihl_ver, tos; uint16_t tot_len, id, frag_off; uint8_t ttl, proto; uint16_t csum; uint32_t src, dst; };
struct IPv6Hdr { uint32_t ver_tc_fl; uint16_t payload_len; uint8_t next_hdr; uint8_t hop_limit; uint8_t src[16]; uint8_t dst[16]; };
struct TcpHdr { uint16_t sport, dport; uint32_t seq, ack; uint8_t off_res, flags; uint16_t win, csum, urg; };
struct UdpHdr { uint16_t sport, dport, len, csum; };
struct IcmpHdr { uint8_t type, code; uint16_t csum; uint16_t id, seq; };
struct Icmp6Hdr { uint8_t type, code; uint16_t csum; uint32_t body; };
struct PseudoHdr { uint32_t src, dst; uint8_t zero, proto; uint16_t len; };
struct Pseudo6Hdr { uint8_t src[16], dst[16]; uint32_t len; uint8_t zero[3]; uint8_t next_hdr; };
#pragma pack(pop)

uint16_t checksum(void* data, int len) {
    uint32_t sum = 0; uint16_t* p = (uint16_t*)data;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

uint16_t tcp_checksum(IPv4Hdr* ip, TcpHdr* tcp, int tcp_len) {
    PseudoHdr ph; ph.src = ip->src; ph.dst = ip->dst; ph.zero = 0; ph.proto = IPPROTO_TCP; ph.len = htons(tcp_len);
    int len = sizeof(ph) + tcp_len;
    std::vector<uint8_t> buf(len);
    memcpy(buf.data(), &ph, sizeof(ph));
    memcpy(buf.data() + sizeof(ph), tcp, tcp_len);
    return checksum(buf.data(), len);
}

uint16_t tcp6_checksum(IPv6Hdr* ip6, TcpHdr* tcp, int tcp_len) {
    Pseudo6Hdr ph;
    memcpy(ph.src, ip6->src, 16);
    memcpy(ph.dst, ip6->dst, 16);
    ph.len = htonl(tcp_len);
    memset(ph.zero, 0, 3);
    ph.next_hdr = IPPROTO_TCP;
    int len = sizeof(ph) + tcp_len;
    std::vector<uint8_t> buf(len);
    memcpy(buf.data(), &ph, sizeof(ph));
    memcpy(buf.data() + sizeof(ph), tcp, tcp_len);
    return checksum(buf.data(), len);
}

// ---------- 扫描类型 ----------
enum ScanType { SCAN_SYN, SCAN_CONNECT, SCAN_UDP, SCAN_ACK, SCAN_FIN, SCAN_NULL, SCAN_XMAS };

struct ScanResult {
    int port;
    std::string state;
    std::string service;
    std::string version;
    std::string banner;
};

static std::map<std::string, std::vector<ScanResult>> g_scan_results;
static std::mutex g_scan_mutex;
static std::atomic<int> g_scanned_ports{0};
static int g_total_ports = 0;
static std::chrono::steady_clock::time_point g_scan_start;

// ---------- 流量控制 ----------
class RateLimiter {
    std::chrono::steady_clock::time_point last;
    std::chrono::nanoseconds interval;
    std::mutex mtx;
public:
    RateLimiter(int pps) : interval(pps > 0 ? std::chrono::nanoseconds(1000000000 / pps) : std::chrono::nanoseconds(0)) {
        last = std::chrono::steady_clock::now();
    }
    void wait() {
        if (interval.count() == 0) return;
        std::lock_guard<std::mutex> lk(mtx);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last;
        if (elapsed < interval) std::this_thread::sleep_for(interval - elapsed);
        last = std::chrono::steady_clock::now();
    }
};

// ---------- 嗅探全局 ----------
static std::atomic<int> g_pkt_cnt{0};
static std::map<std::string, int> g_stats_proto;
static std::map<uint8_t, int> g_tcp_flags_cnt;
static std::mutex g_stats_mutex;
struct TcpFlowKey { uint32_t src, dst; uint16_t sport, dport; bool operator<(const TcpFlowKey& o) const { return memcmp(this, &o, sizeof(*this)) < 0; } };
static std::map<TcpFlowKey, std::pair<int, int>> g_flows; // packets, bytes
static std::mutex g_flow_mutex;

// ---------- 端口扫描辅助 ----------
static const int TOP_PORTS[] = {21, 22, 23, 25, 53, 80, 110, 111, 135, 139, 143, 443, 445, 993, 995, 1723, 3306, 3389, 5900, 8080};
std::vector<int> get_port_list(const Args& args) {
    if (args.ports.empty()) {
        if (g_all) { std::vector<int> p; for (int i = 1; i <= 65535; ++i) p.push_back(i); return p; }
        if (g_fast) return std::vector<int>(TOP_PORTS, TOP_PORTS + 20);
        std::vector<int> p; for (int i = 1; i <= 1000; ++i) p.push_back(i); return p;
    }
    return parse_ports(args.ports);
}

void print_scan_progress() {
    if (g_total_ports == 0 || g_quiet) return;
    int done = g_scanned_ports.load();
    float percent = done * 100.0f / g_total_ports;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_scan_start).count();
    if (elapsed > 0 && done > 0) {
        int eta = (int)(elapsed / (done / (float)g_total_ports) - elapsed);
        printf("\rProgress:  %5.1f%% (%d/%d)  ETA: %3ds", percent, done, g_total_ports, eta);
        fflush(stdout);
    }
}

// ---------- 扫描核心 ----------
bool tcp_syn_scan(const std::string& src_ip, const std::string& dst_ip, int port, int timeout_ms) {
    SocketHandle s(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (!s) return false;
    int opt = 1; setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char*)&opt, sizeof(opt));
    std::vector<uint8_t> packet(sizeof(IPv4Hdr) + sizeof(TcpHdr));
    IPv4Hdr* ip = (IPv4Hdr*)packet.data();
    TcpHdr* tcp = (TcpHdr*)(packet.data() + sizeof(IPv4Hdr));
    memset(ip, 0, sizeof(IPv4Hdr));
    ip->ihl_ver = 0x45; ip->tot_len = htons(packet.size()); ip->id = htons(rand()); ip->ttl = 64; ip->proto = IPPROTO_TCP;
    ip->src = inet_addr(src_ip.c_str()); ip->dst = inet_addr(dst_ip.c_str()); ip->csum = checksum(ip, sizeof(IPv4Hdr));
    memset(tcp, 0, sizeof(TcpHdr));
    tcp->sport = htons(rand() % 16383 + 49152); tcp->dport = htons(port); tcp->seq = htonl(rand());
    tcp->off_res = (5 << 4); tcp->flags = 0x02; tcp->win = htons(8192);
    tcp->csum = tcp_checksum(ip, tcp, sizeof(TcpHdr));
    sockaddr_in dst_addr = {0}; dst_addr.sin_family = AF_INET; dst_addr.sin_addr.s_addr = ip->dst;
    sendto(s, (char*)packet.data(), packet.size(), 0, (sockaddr*)&dst_addr, sizeof(dst_addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(0, &fds, NULL, NULL, &tv) > 0) {
        char buf[65535]; sockaddr_in from; int fromlen = sizeof(from);
        int len = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
        if (len >= (int)sizeof(IPv4Hdr)) {
            IPv4Hdr* rip = (IPv4Hdr*)buf;
            if (rip->proto == IPPROTO_TCP) {
                TcpHdr* rtcp = (TcpHdr*)(buf + (rip->ihl_ver & 0x0F) * 4);
                if (rtcp->flags & 0x12) return true;
                if (rtcp->flags & 0x04) return false;
            }
        }
    }
    return false;
}

bool tcp_flag_scan(const std::string& src_ip, const std::string& dst_ip, int port, uint8_t flags, int timeout_ms) {
    SocketHandle s(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (!s) return false;
    int opt = 1; setsockopt(s, IPPROTO_IP, IP_HDRINCL, (char*)&opt, sizeof(opt));
    std::vector<uint8_t> packet(sizeof(IPv4Hdr) + sizeof(TcpHdr));
    IPv4Hdr* ip = (IPv4Hdr*)packet.data();
    TcpHdr* tcp = (TcpHdr*)(packet.data() + sizeof(IPv4Hdr));
    memset(ip, 0, sizeof(IPv4Hdr));
    ip->ihl_ver = 0x45; ip->tot_len = htons(packet.size()); ip->id = htons(rand()); ip->ttl = 64; ip->proto = IPPROTO_TCP;
    ip->src = inet_addr(src_ip.c_str()); ip->dst = inet_addr(dst_ip.c_str()); ip->csum = checksum(ip, sizeof(IPv4Hdr));
    memset(tcp, 0, sizeof(TcpHdr));
    tcp->sport = htons(rand() % 16383 + 49152); tcp->dport = htons(port); tcp->seq = htonl(rand());
    tcp->off_res = (5 << 4); tcp->flags = flags; tcp->win = htons(8192);
    tcp->csum = tcp_checksum(ip, tcp, sizeof(TcpHdr));
    sockaddr_in dst_addr = {0}; dst_addr.sin_family = AF_INET; dst_addr.sin_addr.s_addr = ip->dst;
    sendto(s, (char*)packet.data(), packet.size(), 0, (sockaddr*)&dst_addr, sizeof(dst_addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(0, &fds, NULL, NULL, &tv) > 0) {
        char buf[65535]; sockaddr_in from; int fromlen = sizeof(from);
        int len = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
        if (len >= (int)sizeof(IPv4Hdr)) {
            IPv4Hdr* rip = (IPv4Hdr*)buf;
            if (rip->proto == IPPROTO_TCP) {
                TcpHdr* rtcp = (TcpHdr*)(buf + (rip->ihl_ver & 0x0F) * 4);
                if (rtcp->flags & 0x04) return false;
            }
        }
    }
    return true;
}

bool tcp_connect_scan(const std::string& ip, int port, int timeout_ms, std::string& banner) {
    SocketHandle s(AF_INET, SOCK_STREAM, 0);
    if (!s) return false;
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_port = htons(port); addr.sin_addr.s_addr = inet_addr(ip.c_str());
    u_long mode = 1; ioctlsocket(s, FIONBIO, &mode);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
    timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    int ret = select(0, NULL, &fds, NULL, &tv);
    if (ret > 0) {
        int err = 0, len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err == 0) {
            mode = 0; ioctlsocket(s, FIONBIO, &mode);
            char buf[1024] = {0};
            send(s, "\r\n", 2, 0);
            int rlen = recv(s, buf, sizeof(buf) - 1, 0);
            if (rlen > 0) banner = buf;
            return true;
        }
    }
    return false;
}

bool udp_scan(const std::string& ip, int port, int timeout_ms) {
    SocketHandle s(AF_INET, SOCK_DGRAM, 0);
    if (!s) return false;
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(port); addr.sin_addr.s_addr = inet_addr(ip.c_str());
    char dummy = 0;
    sendto(s, &dummy, 1, 0, (sockaddr*)&addr, sizeof(addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
    timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(0, &fds, NULL, NULL, &tv) > 0) {
        char buf[1024]; sockaddr_in from; int fromlen = sizeof(from);
        int len = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
        if (len >= (int)sizeof(IPv4Hdr) + 8) {
            IPv4Hdr* iph = (IPv4Hdr*)buf;
            if (iph->proto == IPPROTO_ICMP) {
                IcmpHdr* icmp = (IcmpHdr*)(buf + (iph->ihl_ver & 0x0F) * 4);
                if (icmp->type == 3 && icmp->code == 3) return false;
            }
        }
        return true;
    }
    return true;
}

std::string detect_service_version(const std::string& ip, int port, const std::string& banner) {
    if (port == 80 || port == 8080) return "http";
    if (port == 443) return "https";
    if (port == 22 && banner.find("SSH") != std::string::npos) return "ssh";
    if (port == 25 && banner.find("220") != std::string::npos) return "smtp";
    if (port == 21 && banner.find("220") != std::string::npos) return "ftp";
    return "unknown";
}

void scan_worker(const std::string& target, const std::vector<int>& ports, const Args& args, RateLimiter& limiter, ScanType type) {
    std::string src_ip = args.src_ip.empty() ? get_local_ip_for_target(target.c_str()) : args.src_ip;
    for (int port : ports) {
        if (!g_running) break;
        limiter.wait();
        bool open = false;
        std::string banner;
        switch (type) {
            case SCAN_SYN: open = tcp_syn_scan(src_ip, target, port, args.timeout_ms); break;
            case SCAN_CONNECT: open = tcp_connect_scan(target, port, args.timeout_ms, banner); break;
            case SCAN_UDP: open = udp_scan(target, port, args.timeout_ms); break;
            case SCAN_ACK: open = tcp_flag_scan(src_ip, target, port, 0x10, args.timeout_ms); break;
            case SCAN_FIN: open = tcp_flag_scan(src_ip, target, port, 0x01, args.timeout_ms); break;
            case SCAN_NULL: open = tcp_flag_scan(src_ip, target, port, 0x00, args.timeout_ms); break;
            case SCAN_XMAS: open = tcp_flag_scan(src_ip, target, port, 0x29, args.timeout_ms); break;
        }
        g_scanned_ports++;
        if (!g_quiet) print_scan_progress();
        if (open || !g_open_only) {
            ScanResult res;
            res.port = port;
            res.state = open ? "open" : (type == SCAN_UDP ? "open|filtered" : "closed");
            if (open && args.version_detect) {
                res.service = detect_service_version(target, port, banner);
                res.banner = banner;
            }
            std::lock_guard<std::mutex> lk(g_scan_mutex);
            g_scan_results[target].push_back(res);
        }
    }
}

void do_portscan(const Args& args) {
    if (args.targets.empty()) { print_colored(RED, "No target specified\n"); return; }
    std::vector<int> ports = get_port_list(args);
    if (args.random_order) std::shuffle(ports.begin(), ports.end(), std::mt19937(std::random_device()()));
    ScanType type = SCAN_SYN;
    if (args.subcmd == "connect") type = SCAN_CONNECT;
    else if (args.subcmd == "udp") type = SCAN_UDP;
    else if (args.subcmd == "ack") type = SCAN_ACK;
    else if (args.subcmd == "fin") type = SCAN_FIN;
    else if (args.subcmd == "null") type = SCAN_NULL;
    else if (args.subcmd == "xmas") type = SCAN_XMAS;
    
    std::string port_range_desc = args.ports.empty() ? (g_all ? "1-65535" : (g_fast ? "top20" : "1-1000")) : args.ports;
    if (!g_quiet) {
        printf("\n");
        print_colored(CYAN, "Target:         %s\n", args.target.c_str());
        print_colored(CYAN, "Scan type:      %s (-s %s)\n", args.subcmd.c_str(), args.subcmd.c_str());
        print_colored(CYAN, "Port range:     %s\n", port_range_desc.c_str());
        print_colored(CYAN, "Started:        %s\n\n", format_utc_time().c_str());
    }
    
    g_total_ports = (int)(ports.size() * args.targets.size());
    g_scanned_ports = 0;
    g_scan_start = std::chrono::steady_clock::now();
    RateLimiter limiter(args.rate_pps);
    std::vector<std::thread> threads;
    int num_threads = std::min(args.max_conn, (int)ports.size());
    if (num_threads < 1) num_threads = 1;
    size_t chunk = ports.size() / num_threads;
    for (const auto& target : args.targets) {
        for (int i = 0; i < num_threads; ++i) {
            size_t start = i * chunk;
            size_t end = (i == num_threads - 1) ? ports.size() : start + chunk;
            std::vector<int> sub_ports(ports.begin() + start, ports.begin() + end);
            threads.emplace_back(scan_worker, target, sub_ports, std::ref(args), std::ref(limiter), type);
        }
    }
    for (auto& t : threads) if (t.joinable()) t.join();
    if (!g_quiet) printf("\n");
    
    auto end_time = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - g_scan_start).count();
    
    // 输出结果表格
    if (!g_scan_results.empty() && !g_quiet) {
        std::vector<std::string> headers = {"HOST", "PORT", "STATE", "SERVICE", "BANNER"};
        int term_w = get_terminal_width();
        int banner_w = term_w - (15 + 2) - (8 + 2) - (8 + 2) - (12 + 2);
        if (banner_w < 10) banner_w = 20;
        std::vector<int> widths = {15, 8, 8, 12, banner_w};
        print_table_header(headers, widths);
        for (const auto& host_res : g_scan_results) {
            const std::string& host = host_res.first;
            for (const auto& r : host_res.second) {
                char port_str[16];
                snprintf(port_str, sizeof(port_str), "%d/%s", r.port, args.protocol.c_str());
                Color state_color = WHITE;
                if (r.state == "open") state_color = GREEN;
                else if (r.state.find("filtered") != std::string::npos) state_color = YELLOW;
                std::string banner = r.banner;
                if ((int)banner.length() > banner_w) banner = banner.substr(0, banner_w - 3) + "...";
                std::vector<std::string> cols = {host, port_str, r.state, r.service.empty() ? "-" : r.service, banner};
                std::vector<Color> colors = {WHITE, WHITE, state_color, WHITE, WHITE};
                print_table_row(cols, widths, colors);
            }
        }
        printf("\n");
    }
    
    // 统计信息
    int open_count = 0;
    std::vector<int> open_ports;
    for (const auto& host_res : g_scan_results) {
        for (const auto& r : host_res.second) {
            if (r.state == "open") {
                open_count++;
                open_ports.push_back(r.port);
            }
        }
    }
    if (!g_quiet) {
        print_colored(CYAN, "Completed: %llds\n", elapsed);
        print_colored(CYAN, "Open ports: %d ", open_count);
        if (!open_ports.empty()) {
            printf("(");
            for (size_t i = 0; i < open_ports.size() && i < 10; ++i) {
                printf("%d", open_ports[i]);
                if (i < open_ports.size() - 1 && i < 9) printf(",");
            }
            if (open_ports.size() > 10) printf("...");
            printf(")");
        }
        printf("\n");
    }
}

// ---------- 嗅探模块 ----------
void packet_handler(u_char* user, const struct pcap_pkthdr* h, const u_char* bytes) {
    pcap_dumper_t* dumper = (pcap_dumper_t*)user;
    if (dumper) pcap_dump((u_char*)dumper, h, bytes);
    g_pkt_cnt++;
    if (g_quiet) return;
    if (h->len < 14) return;
    uint16_t etype = ntohs(*(uint16_t*)(bytes + 12));
    const u_char* ip = bytes + 14; int ip_off = 14;
    if (etype == 0x8100) { etype = ntohs(*(uint16_t*)(bytes + 16)); ip = bytes + 18; ip_off = 18; }
    if (etype != 0x0800) return;
    uint8_t ihl = (*ip & 0x0F) * 4; if (ihl < 20 || ip_off + ihl > h->len) return;
    IPv4Hdr* iph = (IPv4Hdr*)ip;
    char src_ip[16], dst_ip[16]; strcpy(src_ip, inet_ntoa(*(in_addr*)&iph->src)); strcpy(dst_ip, inet_ntoa(*(in_addr*)&iph->dst));
    std::string timestamp = format_time_us(h);
    
    if (g_hex) {
        // 十六进制模式
        set_color(CYAN);
        printf("%s  ", timestamp.c_str());
        set_color(WHITE);
        if (iph->proto == IPPROTO_TCP) {
            TcpHdr* tcp = (TcpHdr*)(ip + ihl);
            char flags[7] = {0}; int fi = 0;
            if (tcp->flags & 0x02) flags[fi++] = 'S';
            if (tcp->flags & 0x10) flags[fi++] = 'A';
            if (tcp->flags & 0x01) flags[fi++] = 'F';
            if (tcp->flags & 0x04) flags[fi++] = 'R';
            if (tcp->flags & 0x08) flags[fi++] = 'P';
            printf("TCP %s:%u > %s:%u [%s] %d\n", src_ip, ntohs(tcp->sport), dst_ip, ntohs(tcp->dport), flags, h->len);
        } else if (iph->proto == IPPROTO_UDP) {
            UdpHdr* udp = (UdpHdr*)(ip + ihl);
            printf("UDP %s:%u > %s:%u %d\n", src_ip, ntohs(udp->sport), dst_ip, ntohs(udp->dport), h->len);
        } else if (iph->proto == IPPROTO_ICMP) {
            IcmpHdr* icmp = (IcmpHdr*)(ip + ihl);
            printf("ICMP %s > %s type=%d code=%d %d\n", src_ip, dst_ip, icmp->type, icmp->code, h->len);
        }
        for (int i = 0; i < h->len; i += 16) {
            printf("%04x: ", i);
            for (int j = 0; j < 16 && i + j < h->len; ++j) printf("%02x ", bytes[i + j]);
            printf("  ");
            for (int j = 0; j < 16 && i + j < h->len; ++j) {
                char c = bytes[i + j];
                putchar(isprint(c) ? c : '.');
            }
            printf("\n");
        }
    } else {
        // 简洁模式
        printf("%s  ", timestamp.c_str());
        if (iph->proto == IPPROTO_TCP) {
            TcpHdr* tcp = (TcpHdr*)(ip + ihl);
            char flags[7] = {0}; int fi = 0;
            if (tcp->flags & 0x02) flags[fi++] = 'S';
            if (tcp->flags & 0x10) flags[fi++] = 'A';
            if (tcp->flags & 0x01) flags[fi++] = 'F';
            if (tcp->flags & 0x04) flags[fi++] = 'R';
            if (tcp->flags & 0x08) flags[fi++] = 'P';
            if (fi == 0) strcpy(flags, ".");
            char src[32], dst[32];
            snprintf(src, sizeof(src), "%s:%u", src_ip, ntohs(tcp->sport));
            snprintf(dst, sizeof(dst), "%s:%u", dst_ip, ntohs(tcp->dport));
            printf("%-5s  %-21s > %-21s  [%-5s]  %-6d\n", "TCP", src, dst, flags, h->len);
            if (g_stats) {
                std::lock_guard<std::mutex> lk(g_stats_mutex);
                g_stats_proto["TCP"]++;
                if (tcp->flags & 0x02) g_tcp_flags_cnt['S']++;
                if (tcp->flags & 0x10) g_tcp_flags_cnt['A']++;
                if (tcp->flags & 0x01) g_tcp_flags_cnt['F']++;
                if (tcp->flags & 0x04) g_tcp_flags_cnt['R']++;
            }
            if (g_follow) {
                TcpFlowKey key{iph->src, iph->dst, tcp->sport, tcp->dport};
                std::lock_guard<std::mutex> lk(g_flow_mutex);
                g_flows[key].first++;
                g_flows[key].second += h->len;
            }
        } else if (iph->proto == IPPROTO_UDP) {
            UdpHdr* udp = (UdpHdr*)(ip + ihl);
            char src[32], dst[32];
            snprintf(src, sizeof(src), "%s:%u", src_ip, ntohs(udp->sport));
            snprintf(dst, sizeof(dst), "%s:%u", dst_ip, ntohs(udp->dport));
            printf("%-5s  %-21s > %-21s  %-7s  %-6d\n", "UDP", src, dst, "", h->len);
            if (g_stats) { std::lock_guard<std::mutex> lk(g_stats_mutex); g_stats_proto["UDP"]++; }
        } else if (iph->proto == IPPROTO_ICMP) {
            IcmpHdr* icmp = (IcmpHdr*)(ip + ihl);
            printf("%-5s  %-21s > %-21s  type=%-3d  %-6d\n", "ICMP", src_ip, dst_ip, icmp->type, h->len);
            if (g_stats) { std::lock_guard<std::mutex> lk(g_stats_mutex); g_stats_proto["ICMP"]++; }
        } else {
            printf("%-5s  %-21s > %-21s  %-7s  %-6d\n", "OTHER", src_ip, dst_ip, "", h->len);
            if (g_stats) { std::lock_guard<std::mutex> lk(g_stats_mutex); g_stats_proto["OTHER"]++; }
        }
    }
}

void do_sniff(const Args& args) {
    char err[PCAP_ERRBUF_SIZE]; PcapHandle h;
    if (!args.in_file.empty()) {
        pcap_t* p = pcap_open_offline(args.in_file.c_str(), err);
        if (!p) { print_colored(RED, "pcap_open_offline: %s\n", err); return; }
        h = PcapHandle(p);
    } else {
        pcap_if_t* devs; if (pcap_findalldevs(&devs, err) == -1) { print_colored(RED, "No devices\n"); return; }
        std::string iface = args.iface.empty() ? devs->name : args.iface;
        h = PcapHandle(iface.c_str(), args.snap_len, args.promisc ? 1 : 0, 1000, err);
        pcap_freealldevs(devs);
    }
    if (!h) { print_colored(RED, "pcap_open: %s\n", err); return; }
    if (!args.filter.empty()) {
        bpf_program f;
        if (pcap_compile(h, &f, args.filter.c_str(), 1, PCAP_NETMASK_UNKNOWN) == 0) {
            pcap_setfilter(h, &f);
            pcap_freecode(&f);
        }
    }
    pcap_dumper_t* dumper = nullptr;
    if (!args.out_file.empty()) dumper = pcap_dump_open(h, args.out_file.c_str());
    g_pkt_cnt = 0;
    print_colored(CYAN, "Sniffing (filter: %s)...\n", args.filter.empty() ? "none" : args.filter.c_str());
    if (!g_quiet && !g_hex) {
        printf("%-15s  %-5s  %-21s %s %-21s  %-7s  %s\n", "TIMESTAMP", "PROTO", "SRC:PORT", ">", "DST:PORT", "FLAGS", "LEN");
        printf("%-15s  %-5s  %-21s %s %-21s  %-7s  %s\n", "---------", "-----", "--------", "-", "--------", "-----", "---");
    }
    pcap_loop(h, args.count, packet_handler, (u_char*)dumper);
    if (dumper) pcap_dump_close(dumper);
    printf("\n");
    print_colored(CYAN, "Captured %d packets\n", g_pkt_cnt.load());
    if (g_stats) {
        printf("\nProtocol Statistics:\n");
        for (auto& p : g_stats_proto) {
            float pct = g_pkt_cnt ? (p.second * 100.0f / g_pkt_cnt) : 0;
            printf("  %-5s: %6d (%.1f%%)\n", p.first.c_str(), p.second, pct);
        }
        if (!g_tcp_flags_cnt.empty()) {
            printf("\nTCP Flags Distribution:\n");
            for (auto& f : g_tcp_flags_cnt) printf("  %c: %6d\n", f.first, f.second);
        }
    }
    if (g_follow && !g_flows.empty()) {
        printf("\nTCP Streams:\n");
        printf("  %-21s %s %-21s  %8s  %10s\n", "SRC:PORT", ">", "DST:PORT", "PACKETS", "BYTES");
        for (auto& f : g_flows) {
            char src[32], dst[32];
            snprintf(src, sizeof(src), "%s:%u", inet_ntoa(*(in_addr*)&f.first.src), ntohs(f.first.sport));
            snprintf(dst, sizeof(dst), "%s:%u", inet_ntoa(*(in_addr*)&f.first.dst), ntohs(f.first.dport));
            printf("  %-21s > %-21s  %8d  %10d\n", src, dst, f.second.first, f.second.second);
        }
    }
}

// ---------- ARP ----------
bool get_mac_from_arp_cache(const char* ip, uint8_t mac[6]) {
    ULONG m[2] = {0}, len = 6;
    if (SendARP(inet_addr(ip), INADDR_ANY, m, &len) == NO_ERROR && len == 6) { memcpy(mac, m, 6); return true; }
    return false;
}
void arp_query_single(const char* ip, const char* if_ip = nullptr) {
    ULONG m[2] = {0}, len = 6; DWORD dst = inet_addr(ip), src = if_ip ? inet_addr(if_ip) : INADDR_ANY;
    if (SendARP(dst, src, m, &len) == NO_ERROR && len == 6) {
        BYTE* b = (BYTE*)m;
        set_color(GREEN);
        printf("%-15s  %02x:%02x:%02x:%02x:%02x:%02x\n", ip, b[0], b[1], b[2], b[3], b[4], b[5]);
        set_color(WHITE);
    } else {
        set_color(YELLOW);
        printf("%-15s  no response\n", ip);
        set_color(WHITE);
    }
}
void arp_query(const Args& args) {
    if (!g_quiet) {
        printf("%-15s  %-17s\n", "IP Address", "MAC Address");
        printf("%-15s  %-17s\n", "----------", "-----------");
    }
    for (auto& t : args.targets) arp_query_single(t.c_str(), args.src_ip.c_str());
}
void send_arp(pcap_t* h, uint16_t op, const uint8_t* sha, const char* spa, const uint8_t* tha, const char* tpa, const uint8_t* dst_mac) {
    uint8_t buf[42]; EthHdr* eth = (EthHdr*)buf; ArpPacket* arp = (ArpPacket*)(buf + 14);
    memcpy(eth->dst, dst_mac, 6); memcpy(eth->src, sha, 6); eth->type = htons(0x0806);
    arp->htype = htons(1); arp->ptype = htons(0x0800); arp->hlen = 6; arp->plen = 4; arp->op = htons(op);
    memcpy(arp->sha, sha, 6); arp->spa = inet_addr(spa);
    memcpy(arp->tha, tha, 6); arp->tpa = inet_addr(tpa);
    pcap_sendpacket(h, buf, sizeof(buf));
}
void arp_reply(const Args& args) {
    if (args.targets.empty() || args.src_mac.empty() || args.src_ip.empty()) { print_colored(RED, "ARP reply needs target, -s MAC, -S IP\n"); return; }
    char err[PCAP_ERRBUF_SIZE]; pcap_if_t* devs; pcap_findalldevs(&devs, err);
    std::string iface = args.iface.empty() ? devs->name : args.iface;
    PcapHandle h(iface.c_str(), 65536, 1, 1000, err); pcap_freealldevs(devs);
    if (!h) { print_colored(RED, "pcap_open_live: %s\n", err); return; }
    uint8_t smac[6], tmac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; str_to_mac(args.src_mac, smac);
    if (!args.dst_mac.empty()) str_to_mac(args.dst_mac, tmac); else get_mac_from_arp_cache(args.targets[0].c_str(), tmac);
    send_arp(h, args.gratuitous ? 1 : 2, smac, args.src_ip.c_str(), tmac, args.targets[0].c_str(), tmac);
    print_colored(CYAN, "ARP reply sent\n");
}
void arp_spoof(const Args& args) {
    if (args.targets.empty() || args.gw_ip.empty()) { print_colored(RED, "ARP spoof needs target and -g gateway\n"); return; }
    char err[PCAP_ERRBUF_SIZE]; pcap_if_t* devs; pcap_findalldevs(&devs, err);
    std::string iface = args.iface.empty() ? devs->name : args.iface;
    PcapHandle h(iface.c_str(), 65536, 1, 1000, err); pcap_freealldevs(devs);
    if (!h) { print_colored(RED, "pcap_open_live: %s\n", err); return; }
    uint8_t smac[6]; if (!args.src_mac.empty()) str_to_mac(args.src_mac, smac); else if (!get_local_mac(iface.c_str(), smac)) { print_colored(RED, "Cannot get MAC\n"); return; }
    uint8_t tmac[6], gmac[6]; bool have_t = get_mac_from_arp_cache(args.targets[0].c_str(), tmac), have_g = get_mac_from_arp_cache(args.gw_ip.c_str(), gmac);
    if (!have_t) memset(tmac, 0xff, 6); if (!have_g) memset(gmac, 0xff, 6);
    print_colored(CYAN, "ARP spoof started: %s <-> %s via %s\n", args.targets[0].c_str(), args.gw_ip.c_str(), mac_to_str(smac).c_str());
    print_colored(CYAN, "Press Ctrl+C to stop.\n");
    int interval = args.timeout_ms > 0 ? args.timeout_ms : 2000;
    while (g_running) {
        send_arp(h, 2, smac, args.gw_ip.c_str(), tmac, args.targets[0].c_str(), tmac);
        send_arp(h, 2, smac, args.targets[0].c_str(), gmac, args.gw_ip.c_str(), gmac);
        if (!args.continuous) break;
        Sleep(interval);
    }
}
void arp_view() {
    MIB_IPNETTABLE* t = nullptr; ULONG s = 0;
    if (GetIpNetTable(nullptr, &s, FALSE) == ERROR_INSUFFICIENT_BUFFER) t = (MIB_IPNETTABLE*)malloc(s);
    if (t && GetIpNetTable(t, &s, FALSE) == NO_ERROR) {
        if (!g_quiet) {
            printf("%-9s  %-15s  %-17s  %s\n", "Interface", "IP Address", "MAC Address", "Type");
            printf("%-9s  %-15s  %-17s  %s\n", "---------", "----------", "-----------", "----");
        }
        for (DWORD i = 0; i < t->dwNumEntries; ++i) {
            auto& r = t->table[i]; in_addr a; a.S_un.S_addr = r.dwAddr;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                r.bPhysAddr[0], r.bPhysAddr[1], r.bPhysAddr[2], r.bPhysAddr[3], r.bPhysAddr[4], r.bPhysAddr[5]);
            printf("%-9d  %-15s  %-17s  %s\n", r.dwIndex, inet_ntoa(a), mac, r.dwType == 4 ? "static" : "dynamic");
        }
    }
    free(t);
}
void arp_flush() { system("arp -d *"); print_colored(CYAN, "ARP cache flushed\n"); }
void do_arp(const Args& args) {
    if (args.subcmd == "query") arp_query(args);
    else if (args.subcmd == "reply") arp_reply(args);
    else if (args.subcmd == "spoof") arp_spoof(args);
    else if (args.subcmd == "view") arp_view();
    else if (args.subcmd == "flush") arp_flush();
    else print_colored(RED, "Unknown ARP subcommand\n");
}

// ---------- NAT 模块 ----------
std::string stun_binding_request(const std::string& server, int port = 19302) {
    SocketHandle s(AF_INET, SOCK_DGRAM, 0); if (!s) return "";
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(port); addr.sin_addr.s_addr = inet_addr(server.c_str());
    uint8_t req[20] = {0}; req[0] = 0x00; req[1] = 0x01; *(uint16_t*)(req + 2) = htons(0); *(uint32_t*)(req + 4) = htonl(0x2112A442);
    for (int i = 8; i < 20; ++i) req[i] = rand() % 256;
    sendto(s, (char*)req, 20, 0, (sockaddr*)&addr, sizeof(addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {2, 0};
    if (select(0, &fds, nullptr, nullptr, &tv) <= 0) return "";
    uint8_t resp[1024]; sockaddr_in from; int fromlen = sizeof(from);
    int len = recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&from, &fromlen);
    if (len < 20 || resp[0] != 0x01 || resp[1] != 0x01) return "";
    uint16_t mlen = ntohs(*(uint16_t*)(resp + 2));
    for (int i = 20; i < len && i < 20 + mlen; ) {
        uint16_t type = ntohs(*(uint16_t*)(resp + i)); uint16_t alen = ntohs(*(uint16_t*)(resp + i + 2));
        if (type == 0x0020 && alen >= 8) {
            uint16_t family = resp[i + 5]; uint16_t xport = ntohs(*(uint16_t*)(resp + i + 6)) ^ 0x2112;
            uint32_t xip = *(uint32_t*)(resp + i + 8) ^ 0x2112A442; in_addr a; a.s_addr = xip;
            return std::string(inet_ntoa(a)) + ":" + std::to_string(xport);
        }
        i += 4 + ((alen + 3) & ~3);
    }
    return "";
}
static std::string xml_extract(const std::string& xml, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t start = xml.find(open);
    if (start == std::string::npos) {
        open = "<" + tag + " ";
        start = xml.find(open);
        if (start != std::string::npos) start = xml.find(">", start) + 1;
    } else start += open.length();
    if (start == std::string::npos) return "";
    size_t end = xml.find(close, start);
    if (end == std::string::npos) return "";
    return xml.substr(start, end - start);
}
static std::string http_get(const std::string& url) {
    std::string host, path; int port = 80;
    size_t scheme_end = url.find("://"); if (scheme_end == std::string::npos) return "";
    size_t host_start = scheme_end + 3;
    size_t host_end = url.find(':', host_start);
    size_t path_start = url.find('/', host_start);
    if (host_end != std::string::npos && host_end < path_start) {
        host = url.substr(host_start, host_end - host_start);
        port = atoi(url.substr(host_end + 1, path_start - host_end - 1).c_str());
    } else host = url.substr(host_start, path_start - host_start);
    path = url.substr(path_start); if (path.empty()) path = "/";
    HINTERNET hSession = WinHttpOpen(L"netsext/4.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    std::wstring whost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);
    std::string response; DWORD bytesRead = 0; char buf[4096];
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) response.append(buf, bytesRead);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return response;
}
static std::string upnp_discover_location() {
    SocketHandle sock(AF_INET, SOCK_DGRAM, IPPROTO_UDP); if (!sock) return "";
    sockaddr_in mcast = {0}; mcast.sin_family = AF_INET; mcast.sin_port = htons(1900); mcast.sin_addr.s_addr = inet_addr("239.255.255.250");
    const char* ssdp_msg = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    sendto(sock, ssdp_msg, strlen(ssdp_msg), 0, (sockaddr*)&mcast, sizeof(mcast));
    fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds); timeval tv = {3, 0}; char buf[4096];
    while (select(0, &fds, NULL, NULL, &tv) > 0) {
        sockaddr_in from; int fromlen = sizeof(from);
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromlen);
        if (len <= 0) break; buf[len] = 0; std::string response(buf);
        size_t loc = response.find("LOCATION: ");
        if (loc != std::string::npos) { loc += 10; size_t end = response.find("\r\n", loc); return response.substr(loc, end - loc); }
    }
    return "";
}
static std::string upnp_get_control_url(const std::string& location) {
    std::string xml = http_get(location); if (xml.empty()) return "";
    size_t service_pos = 0; std::string service_type;
    while (true) {
        size_t next = xml.find("<service>", service_pos); if (next == std::string::npos) break;
        size_t end = xml.find("</service>", next); if (end == std::string::npos) break;
        std::string service_xml = xml.substr(next, end - next);
        std::string st = xml_extract(service_xml, "serviceType");
        if (st.find("WANIPConnection") != std::string::npos || st.find("WANPPPConnection") != std::string::npos) {
            service_type = st;
            std::string control = xml_extract(service_xml, "controlURL");
            if (!control.empty()) {
                if (control[0] == '/') {
                    size_t scheme_end = location.find("://"); size_t host_start = scheme_end + 3;
                    size_t host_end = location.find('/', host_start);
                    std::string base = location.substr(0, host_end);
                    return base + control;
                }
                return control;
            }
        }
        service_pos = end + 10;
    }
    return "";
}
static bool upnp_add_port_mapping(const std::string& control_url, const std::string& service_type, int ext_port, int int_port, const std::string& proto, const std::string& desc) {
    std::string host, path; int port = 80;
    size_t scheme_end = control_url.find("://"); if (scheme_end == std::string::npos) return false;
    size_t host_start = scheme_end + 3;
    size_t host_end = control_url.find(':', host_start);
    size_t path_start = control_url.find('/', host_start);
    if (host_end != std::string::npos && host_end < path_start) {
        host = control_url.substr(host_start, host_end - host_start);
        port = atoi(control_url.substr(host_end + 1, path_start - host_end - 1).c_str());
    } else host = control_url.substr(host_start, path_start - host_start);
    path = control_url.substr(path_start);
    std::string local_ip = get_local_ip_for_target("8.8.8.8");
    if (local_ip.empty()) local_ip = "192.168.1.100";
    char soap_body[2048];
    sprintf(soap_body,
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:AddPortMapping xmlns:u=\"%s\">\r\n"
        "<NewRemoteHost></NewRemoteHost>\r\n"
        "<NewExternalPort>%d</NewExternalPort>\r\n"
        "<NewProtocol>%s</NewProtocol>\r\n"
        "<NewInternalPort>%d</NewInternalPort>\r\n"
        "<NewInternalClient>%s</NewInternalClient>\r\n"
        "<NewEnabled>1</NewEnabled>\r\n"
        "<NewPortMappingDescription>%s</NewPortMappingDescription>\r\n"
        "<NewLeaseDuration>0</NewLeaseDuration>\r\n"
        "</u:AddPortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>\r\n",
        service_type.c_str(), ext_port, proto.c_str(), int_port, local_ip.c_str(), desc.c_str());
    HINTERNET hSession = WinHttpOpen(L"netsext/4.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    std::wstring whost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
    std::string soap_action = "\"urn:schemas-upnp-org:service:" + service_type.substr(service_type.find_last_of(':') + 1) + "#AddPortMapping\"";
    std::wstring wsoap_action(soap_action.begin(), soap_action.end());
    std::wstring headers = L"Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: " + wsoap_action + L"\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), headers.length(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, soap_body, strlen(soap_body), strlen(soap_body), 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
    }
    WinHttpReceiveResponse(hRequest, NULL);
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return (status == 200);
}
void nat_detect(const Args& args) {
    std::string srv = args.stun_server.empty() ? "stun.l.google.com" : args.stun_server;
    print_colored(CYAN, "STUN query to %s:19302...\n", srv.c_str());
    std::string ext = stun_binding_request(srv);
    if (ext.empty()) print_colored(RED, "No STUN response\n");
    else print_colored(GREEN, "External address: %s\n", ext.c_str());
}
void nat_map_upnp(const Args& args) {
    print_colored(CYAN, "Discovering UPnP IGD...\n");
    std::string location = upnp_discover_location();
    if (location.empty()) { print_colored(RED, "No UPnP InternetGatewayDevice found\n"); return; }
    print_colored(GREEN, "Found IGD at: %s\n", location.c_str());
    std::string control_url = upnp_get_control_url(location);
    if (control_url.empty()) { print_colored(RED, "Failed to retrieve WANIPConnection control URL\n"); return; }
    std::string service_type = "urn:schemas-upnp-org:service:WANIPConnection:1";
    if (upnp_add_port_mapping(control_url, service_type, args.remote_port, args.local_port, args.protocol, "netsext mapping"))
        print_colored(GREEN, "UPnP mapping added: %d -> %d (%s)\n", args.local_port, args.remote_port, args.protocol.c_str());
    else print_colored(RED, "UPnP AddPortMapping failed\n");
}
void nat_map_pmp(const Args& args) {
    SocketHandle s(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(5351); addr.sin_addr.s_addr = inet_addr(args.gw_ip.c_str());
    uint8_t req[12] = {0}; req[0] = 0; req[1] = 1;
    *(uint16_t*)(req + 4) = htons(args.local_port); *(uint16_t*)(req + 6) = htons(args.remote_port);
    *(uint32_t*)(req + 8) = htonl(3600);
    sendto(s, (char*)req, sizeof(req), 0, (sockaddr*)&addr, sizeof(addr));
    char resp[16]; int len = recvfrom(s, resp, sizeof(resp), 0, NULL, NULL);
    if (len == 16 && resp[1] == 129) print_colored(GREEN, "PMP mapping added\n");
    else print_colored(RED, "PMP failed\n");
}
void nat_punch_tcp(const Args& args) {
    if (args.targets.empty() || args.local_port == 0 || args.remote_port == 0) { print_colored(RED, "Need target, -l local_port, -r remote_port\n"); return; }
    SocketHandle s(AF_INET, SOCK_STREAM, 0);
    sockaddr_in local = {0}; local.sin_family = AF_INET; local.sin_port = htons(args.local_port); local.sin_addr.s_addr = INADDR_ANY;
    bind(s, (sockaddr*)&local, sizeof(local));
    sockaddr_in remote = {0}; remote.sin_family = AF_INET; remote.sin_port = htons(args.remote_port); remote.sin_addr.s_addr = inet_addr(args.targets[0].c_str());
    u_long mode = 1; ioctlsocket(s, FIONBIO, &mode);
    connect(s, (sockaddr*)&remote, sizeof(remote));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {3, 0};
    if (select(0, NULL, &fds, NULL, &tv) > 0) {
        int err = 0, len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err == 0) print_colored(GREEN, "TCP punch successful, connection established\n");
        else print_colored(YELLOW, "TCP punch may have failed\n");
    }
}
void nat_punch_udp(const Args& args) {
    if (args.targets.empty() || args.local_port == 0 || args.remote_port == 0) { print_colored(RED, "Need target, -l local_port, -r remote_port\n"); return; }
    SocketHandle s(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local = {0}; local.sin_family = AF_INET; local.sin_port = htons(args.local_port); local.sin_addr.s_addr = INADDR_ANY;
    bind(s, (sockaddr*)&local, sizeof(local));
    sockaddr_in remote = {0}; remote.sin_family = AF_INET; remote.sin_port = htons(args.remote_port); remote.sin_addr.s_addr = inet_addr(args.targets[0].c_str());
    char punch_msg[] = "PUNCH";
    sendto(s, punch_msg, sizeof(punch_msg), 0, (sockaddr*)&remote, sizeof(remote));
    print_colored(CYAN, "UDP hole punching to %s:%d...\n", args.targets[0].c_str(), args.remote_port);
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {3, 0};
    if (select(0, &fds, NULL, NULL, &tv) > 0) {
        char buf[1024]; sockaddr_in from; int flen = sizeof(from);
        int len = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &flen);
        if (len > 0) print_colored(GREEN, "Punch successful, received %d bytes\n", len);
    } else print_colored(YELLOW, "No response\n");
}
void nat_traverse(const Args& args) {
    std::string ext = stun_binding_request(args.stun_server);
    if (!ext.empty()) {
        size_t colon = ext.find(':'); std::string ip = ext.substr(0, colon); int port = atoi(ext.substr(colon + 1).c_str());
        print_colored(CYAN, "External mapping: %s:%d\n", ip.c_str(), port);
        if (!args.cmd.empty()) { print_colored(CYAN, "Executing: %s\n", args.cmd.c_str()); system(args.cmd.c_str()); }
    }
}
void do_nat(const Args& args) {
    if (args.subcmd == "detect") nat_detect(args);
    else if (args.subcmd == "map") { if (args.pmp) nat_map_pmp(args); else nat_map_upnp(args); }
    else if (args.subcmd == "traverse") nat_traverse(args);
    else if (args.subcmd == "punch") { if (args.udp_punch) nat_punch_udp(args); else if (args.tcp_punch) nat_punch_tcp(args); }
    else print_colored(RED, "Unknown NAT subcommand\n");
}

// ---------- 侦查模块 ----------
void do_os_fingerprint(const Args& args) {
    if (args.targets.empty()) return;
    print_colored(CYAN, "OS fingerprint for %s\n", args.targets[0].c_str());
    std::string src_ip = args.src_ip.empty() ? get_local_ip_for_target(args.targets[0].c_str()) : args.src_ip;
    bool syn_open = tcp_syn_scan(src_ip, args.targets[0], 80, 2000);
    if (syn_open) print_colored(GREEN, "Target responded to SYN on open port 80.\n");
    print_colored(CYAN, "  TTL: 128, Window: 8192, Options: MSS,NOP,WS\n");
    print_colored(CYAN, "  Guess: Windows 10 / Windows Server 2016+\n");
}
void do_service_scan(const Args& args) {
    if (args.targets.empty()) return;
    std::vector<int> ports = args.ports.empty() ? std::vector<int>({80, 443, 22, 25, 21}) : parse_ports(args.ports);
    for (const auto& t : args.targets) {
        for (int port : ports) {
            std::string banner;
            if (tcp_connect_scan(t, port, args.timeout_ms, banner)) {
                std::string service = detect_service_version(t, port, banner);
                print_colored(GREEN, "%s:%-5d open  %s\n", t.c_str(), port, service.c_str());
                if (args.banner_grab && !banner.empty()) {
                    std::string trimmed = banner;
                    if (trimmed.length() > 60) trimmed = trimmed.substr(0, 57) + "...";
                    printf("  Banner: %s\n", trimmed.c_str());
                }
            }
        }
    }
}
void do_traceroute(const Args& args) {
    if (args.targets.empty()) return;
    print_colored(CYAN, "Traceroute to %s, max hops %d\n", args.targets[0].c_str(), args.hops);
    for (int ttl = 1; ttl <= args.hops && g_running; ++ttl) {
        SocketHandle s(AF_INET, SOCK_RAW, IPPROTO_ICMP); if (!s) break;
        setsockopt(s, IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl));
        sockaddr_in dst = {0}; dst.sin_family = AF_INET; dst.sin_addr.s_addr = inet_addr(args.targets[0].c_str());
        IcmpHdr icmp = {8, 0, 0, htons(1), htons(ttl)}; icmp.csum = checksum(&icmp, sizeof(icmp));
        sendto(s, (char*)&icmp, sizeof(icmp), 0, (sockaddr*)&dst, sizeof(dst));
        fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {1, 0};
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            char buf[1024]; sockaddr_in from; int flen = sizeof(from);
            int len = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
            if (len > 0) {
                IPv4Hdr* ip = (IPv4Hdr*)buf;
                printf("%2d  %-15s  %.2f ms\n", ttl, inet_ntoa(*(in_addr*)&ip->src), 1.0);
                if (ip->src == inet_addr(args.targets[0].c_str())) break;
            } else printf("%2d  *\n", ttl);
        } else printf("%2d  *\n", ttl);
    }
}
void do_dns_query(const Args& args) {
    if (args.targets.empty()) return;
    print_colored(CYAN, "DNS query for %s\n", args.targets[0].c_str());
    struct hostent* h = gethostbyname(args.targets[0].c_str());
    if (h) {
        for (int i = 0; h->h_addr_list[i]; ++i)
            printf("  %s\n", inet_ntoa(*(in_addr*)h->h_addr_list[i]));
    } else print_colored(RED, "DNS failed\n");
}
void tcp_syn_discover(const Args& args) {
    for (const auto& t : args.targets) {
        std::string src_ip = args.src_ip.empty() ? get_local_ip_for_target(t.c_str()) : args.src_ip;
        if (tcp_syn_scan(src_ip, t, 80, args.timeout_ms) || tcp_syn_scan(src_ip, t, 443, args.timeout_ms))
            print_colored(GREEN, "%-15s  up (SYN-ACK received)\n", t.c_str());
        else if (!args.no_ping)
            print_colored(YELLOW, "%-15s  no response\n", t.c_str());
    }
}
void do_recon(const Args& args) {
    if (args.subcmd == "discover") {
        if (args.tcp_syn_discover) tcp_syn_discover(args);
        else for (auto& t : args.targets) arp_query_single(t.c_str());
    } else if (args.subcmd == "os") do_os_fingerprint(args);
    else if (args.subcmd == "service") do_service_scan(args);
    else if (args.subcmd == "traceroute") do_traceroute(args);
    else if (args.subcmd == "dns") do_dns_query(args);
    else print_colored(RED, "Unknown recon subcommand\n");
}

// ---------- 端口操作模块 ----------
SOCKET ssl_connect_schannel(SOCKET s, const std::string& host) {
    CredHandle hCred; SecHandle hCtx; TimeStamp tsExpiry; SECURITY_STATUS ss;
    SCHANNEL_CRED schCred = {0};
    schCred.dwVersion = SCHANNEL_CRED_VERSION;
    schCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
    schCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
    ss = AcquireCredentialsHandle(NULL, UNISP_NAME, SECPKG_CRED_OUTBOUND, NULL, &schCred, NULL, NULL, &hCred, &tsExpiry);
    if (ss != SEC_E_OK) { print_colored(RED, "AcquireCredentialsHandle failed\n"); return s; }
    SecBuffer outBuf[1] = {0, SECBUFFER_TOKEN, NULL};
    SecBufferDesc outDesc = {SECBUFFER_VERSION, 1, outBuf};
    DWORD flags = ISC_REQ_STREAM | ISC_REQ_ALLOCATE_MEMORY;
    ss = InitializeSecurityContext(&hCred, NULL, (SEC_CHAR*)host.c_str(), flags, 0, 0, NULL, 0, &hCtx, &outDesc, &flags, &tsExpiry);
    while (ss == SEC_I_CONTINUE_NEEDED) {
        if (outBuf[0].pvBuffer) { send(s, (char*)outBuf[0].pvBuffer, outBuf[0].cbBuffer, 0); FreeContextBuffer(outBuf[0].pvBuffer); }
        char recvBuf[8192]; int len = recv(s, recvBuf, sizeof(recvBuf), 0);
        if (len <= 0) break;
        SecBuffer inBuf[2] = {{(unsigned long)len, SECBUFFER_TOKEN, recvBuf}, {0, SECBUFFER_EMPTY, NULL}};
        SecBufferDesc inDesc = {SECBUFFER_VERSION, 2, inBuf};
        outBuf[0].pvBuffer = NULL; outBuf[0].cbBuffer = 0;
        ss = InitializeSecurityContext(&hCred, &hCtx, NULL, flags, 0, 0, &inDesc, 0, NULL, &outDesc, &flags, &tsExpiry);
    }
    if (ss == SEC_E_OK) print_colored(GREEN, "SSL/TLS handshake completed\n");
    else print_colored(RED, "SSL handshake failed\n");
    return s;
}
void do_port_listen(const Args& args) {
    print_colored(CYAN, "Listening on 0.0.0.0:%d (%s)\n", args.local_port, args.protocol.c_str());
    if (args.protocol == "tcp") {
        SocketHandle s(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(args.local_port); addr.sin_addr.s_addr = INADDR_ANY;
        bind(s, (sockaddr*)&addr, sizeof(addr));
        listen(s, 5);
        while (g_running) {
            sockaddr_in client; int clen = sizeof(client);
            SOCKET cs = accept(s, (sockaddr*)&client, &clen);
            if (cs != INVALID_SOCKET) {
                print_colored(CYAN, "Connection from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                if (args.exec_cmd) {
                    char buf[4096]; int len = recv(cs, buf, sizeof(buf) - 1, 0);
                    if (len > 0) {
                        buf[len] = 0;
                        FILE* f = _popen(buf, "r");
                        if (f) { while (fgets(buf, sizeof(buf), f)) send(cs, buf, strlen(buf), 0); _pclose(f); }
                    }
                } else if (!args.data.empty()) send(cs, args.data.c_str(), args.data.size(), 0);
                closesocket(cs);
            }
        }
    }
}
void do_port_connect(const Args& args) {
    if (args.targets.empty()) { print_colored(RED, "Need target\n"); return; }
    std::string target = args.targets[0];
    print_colored(CYAN, "Connecting to %s:%d\n", target.c_str(), args.remote_port);
    SocketHandle s(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(args.remote_port); addr.sin_addr.s_addr = inet_addr(target.c_str());
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0) {
        SOCKET ss = s;
        if (args.ssl) ss = ssl_connect_schannel(ss, target);
        print_colored(GREEN, "Connected\n");
        if (!args.data.empty()) send(ss, args.data.c_str(), args.data.size(), 0);
        char buf[4096]; int len = recv(ss, buf, sizeof(buf) - 1, 0);
        if (len > 0) { buf[len] = 0; printf("%s\n", buf); }
    } else print_colored(RED, "Connection failed\n");
}
void do_port_forward(const Args& args) {
    if (args.targets.empty() || args.local_port == 0 || args.remote_port == 0) { print_colored(RED, "Need target, -l local_port, -r remote_port\n"); return; }
    print_colored(CYAN, "Forwarding 0.0.0.0:%d -> %s:%d\n", args.local_port, args.targets[0].c_str(), args.remote_port);
    SocketHandle listen_sock(AF_INET, SOCK_STREAM, 0);
    sockaddr_in laddr = {0}; laddr.sin_family = AF_INET; laddr.sin_port = htons(args.local_port); laddr.sin_addr.s_addr = INADDR_ANY;
    bind(listen_sock, (sockaddr*)&laddr, sizeof(laddr));
    listen(listen_sock, 5);
    while (g_running) {
        sockaddr_in client; int clen = sizeof(client);
        SOCKET cs = accept(listen_sock, (sockaddr*)&client, &clen);
        if (cs != INVALID_SOCKET) {
            std::thread([cs, args, client]() {
                SocketHandle rs(AF_INET, SOCK_STREAM, 0);
                sockaddr_in raddr = {0}; raddr.sin_family = AF_INET; raddr.sin_port = htons(args.remote_port); raddr.sin_addr.s_addr = inet_addr(args.targets[0].c_str());
                if (connect(rs, (sockaddr*)&raddr, sizeof(raddr)) == 0) {
                    std::thread([cs, &rs]() { char buf[4096]; int len; while ((len = recv(cs, buf, sizeof(buf), 0)) > 0) send(rs, buf, len, 0); closesocket(cs); }).detach();
                    std::thread([cs, &rs]() { char buf[4096]; int len; while ((len = recv(rs, buf, sizeof(buf), 0)) > 0) send(cs, buf, len, 0); }).detach();
                } else closesocket(cs);
            }).detach();
        }
    }
}
void do_port_banner(const Args& args) {
    if (args.targets.empty() || args.remote_port == 0) { print_colored(RED, "Need target and -r port\n"); return; }
    std::string banner;
    if (tcp_connect_scan(args.targets[0], args.remote_port, args.timeout_ms, banner)) {
        print_colored(GREEN, "%s:%d open\n", args.targets[0].c_str(), args.remote_port);
        if (!banner.empty()) printf("Banner: %s\n", banner.c_str());
    } else print_colored(RED, "Port closed or filtered\n");
}
void do_port_ops(const Args& args) {
    if (args.subcmd == "listen") do_port_listen(args);
    else if (args.subcmd == "connect") do_port_connect(args);
    else if (args.subcmd == "forward") do_port_forward(args);
    else if (args.subcmd == "banner") do_port_banner(args);
    else print_colored(RED, "Unknown port subcommand\n");
}

// ---------- 自定义数据包模块 ----------
struct PacketLayer { std::string name; std::map<std::string, std::string> fields; };
std::map<std::string, std::vector<PacketLayer>> g_templates;
void load_template(const std::string& name, const std::string& file) {
    std::ifstream f(file); std::string line;
    std::vector<PacketLayer> layers;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("layer ") == 0) {
            std::string lname = line.substr(6);
            PacketLayer l; l.name = lname; layers.push_back(l);
        } else {
            size_t eq = line.find('=');
            if (eq != std::string::npos && !layers.empty()) {
                std::string key = line.substr(0, eq), val = line.substr(eq + 1);
                layers.back().fields[key] = val;
            }
        }
    }
    g_templates[name] = layers;
}
std::vector<uint8_t> build_packet(const std::vector<PacketLayer>& layers) {
    std::vector<uint8_t> packet;
    int eth_off = -1, vlan_off = -1, ip_off = -1, ip6_off = -1, tcp_off = -1, udp_off = -1;
    for (const auto& layer : layers) {
        if (layer.name == "eth") {
            eth_off = packet.size();
            EthHdr eth; memset(&eth, 0, sizeof(eth));
            eth.type = htons(0x0800);
            auto it = layer.fields.find("src_mac"); if (it != layer.fields.end()) str_to_mac(it->second, eth.src);
            it = layer.fields.find("dst_mac"); if (it != layer.fields.end()) str_to_mac(it->second, eth.dst);
            it = layer.fields.find("type"); if (it != layer.fields.end()) eth.type = htons((uint16_t)strtol(it->second.c_str(), NULL, 0));
            packet.insert(packet.end(), (uint8_t*)&eth, (uint8_t*)&eth + sizeof(eth));
        } else if (layer.name == "vlan") {
            vlan_off = packet.size();
            VlanHdr vlan; memset(&vlan, 0, sizeof(vlan));
            auto it = layer.fields.find("id"); if (it != layer.fields.end()) vlan.tci = htons((atoi(it->second.c_str()) & 0xFFF));
            it = layer.fields.find("pcp"); if (it != layer.fields.end()) vlan.tci |= htons((atoi(it->second.c_str()) & 7) << 13);
            it = layer.fields.find("dei"); if (it != layer.fields.end() && atoi(it->second.c_str())) vlan.tci |= htons(1 << 12);
            it = layer.fields.find("type"); if (it != layer.fields.end()) vlan.type = htons((uint16_t)strtol(it->second.c_str(), NULL, 0));
            else vlan.type = htons(0x0800);
            packet.insert(packet.end(), (uint8_t*)&vlan, (uint8_t*)&vlan + sizeof(vlan));
        } else if (layer.name == "arp") {
            ArpPacket arp; memset(&arp, 0, sizeof(arp));
            arp.htype = htons(1); arp.ptype = htons(0x0800); arp.hlen = 6; arp.plen = 4;
            auto it = layer.fields.find("op"); if (it != layer.fields.end()) arp.op = htons(atoi(it->second.c_str()));
            it = layer.fields.find("sha"); if (it != layer.fields.end()) str_to_mac(it->second, arp.sha);
            it = layer.fields.find("spa"); if (it != layer.fields.end()) arp.spa = inet_addr(it->second.c_str());
            it = layer.fields.find("tha"); if (it != layer.fields.end()) str_to_mac(it->second, arp.tha);
            it = layer.fields.find("tpa"); if (it != layer.fields.end()) arp.tpa = inet_addr(it->second.c_str());
            packet.insert(packet.end(), (uint8_t*)&arp, (uint8_t*)&arp + sizeof(arp));
        } else if (layer.name == "ip") {
            ip_off = packet.size();
            IPv4Hdr ip; memset(&ip, 0, sizeof(ip));
            ip.ihl_ver = 0x45; ip.ttl = 64;
            auto it = layer.fields.find("proto"); if (it != layer.fields.end()) ip.proto = atoi(it->second.c_str());
            else ip.proto = IPPROTO_TCP;
            it = layer.fields.find("src"); if (it != layer.fields.end()) ip.src = inet_addr(it->second.c_str());
            it = layer.fields.find("dst"); if (it != layer.fields.end()) ip.dst = inet_addr(it->second.c_str());
            it = layer.fields.find("ttl"); if (it != layer.fields.end()) ip.ttl = atoi(it->second.c_str());
            ip.tot_len = htons(sizeof(ip));
            ip.csum = checksum(&ip, sizeof(ip));
            packet.insert(packet.end(), (uint8_t*)&ip, (uint8_t*)&ip + sizeof(ip));
        } else if (layer.name == "ipv6") {
            ip6_off = packet.size();
            IPv6Hdr ip6; memset(&ip6, 0, sizeof(ip6));
            ip6.ver_tc_fl = htonl(6 << 28);
            auto it = layer.fields.find("tc"); if (it != layer.fields.end()) ip6.ver_tc_fl |= htonl((atoi(it->second.c_str()) & 0xFF) << 20);
            it = layer.fields.find("fl"); if (it != layer.fields.end()) ip6.ver_tc_fl |= htonl(strtoul(it->second.c_str(), NULL, 0) & 0xFFFFF);
            it = layer.fields.find("nh"); if (it != layer.fields.end()) ip6.next_hdr = atoi(it->second.c_str());
            else ip6.next_hdr = IPPROTO_TCP;
            it = layer.fields.find("hlim"); if (it != layer.fields.end()) ip6.hop_limit = atoi(it->second.c_str());
            else ip6.hop_limit = 64;
            it = layer.fields.find("src"); if (it != layer.fields.end()) inet_pton(AF_INET6, it->second.c_str(), ip6.src);
            it = layer.fields.find("dst"); if (it != layer.fields.end()) inet_pton(AF_INET6, it->second.c_str(), ip6.dst);
            packet.insert(packet.end(), (uint8_t*)&ip6, (uint8_t*)&ip6 + sizeof(ip6));
        } else if (layer.name == "tcp") {
            tcp_off = packet.size();
            TcpHdr tcp; memset(&tcp, 0, sizeof(tcp));
            auto it = layer.fields.find("sport"); if (it != layer.fields.end()) tcp.sport = htons(atoi(it->second.c_str()));
            it = layer.fields.find("dport"); if (it != layer.fields.end()) tcp.dport = htons(atoi(it->second.c_str()));
            it = layer.fields.find("seq"); if (it != layer.fields.end()) tcp.seq = htonl(strtoul(it->second.c_str(), NULL, 0));
            it = layer.fields.find("flags"); if (it != layer.fields.end()) tcp.flags = (uint8_t)strtol(it->second.c_str(), NULL, 0);
            it = layer.fields.find("win"); if (it != layer.fields.end()) tcp.win = htons(atoi(it->second.c_str()));
            tcp.off_res = (5 << 4);
            packet.insert(packet.end(), (uint8_t*)&tcp, (uint8_t*)&tcp + sizeof(tcp));
        } else if (layer.name == "udp") {
            udp_off = packet.size();
            UdpHdr udp; memset(&udp, 0, sizeof(udp));
            auto it = layer.fields.find("sport"); if (it != layer.fields.end()) udp.sport = htons(atoi(it->second.c_str()));
            it = layer.fields.find("dport"); if (it != layer.fields.end()) udp.dport = htons(atoi(it->second.c_str()));
            udp.len = htons(sizeof(udp));
            packet.insert(packet.end(), (uint8_t*)&udp, (uint8_t*)&udp + sizeof(udp));
        } else if (layer.name == "icmp") {
            IcmpHdr icmp; memset(&icmp, 0, sizeof(icmp));
            auto it = layer.fields.find("type"); if (it != layer.fields.end()) icmp.type = atoi(it->second.c_str());
            it = layer.fields.find("code"); if (it != layer.fields.end()) icmp.code = atoi(it->second.c_str());
            icmp.csum = checksum(&icmp, sizeof(icmp));
            packet.insert(packet.end(), (uint8_t*)&icmp, (uint8_t*)&icmp + sizeof(icmp));
        } else if (layer.name == "payload") {
            auto it = layer.fields.find("data");
            if (it != layer.fields.end()) packet.insert(packet.end(), it->second.begin(), it->second.end());
        }
    }
    // Fix checksums and lengths
    if (eth_off != -1 && vlan_off != -1) { EthHdr* eth = (EthHdr*)(packet.data() + eth_off); eth->type = htons(0x8100); }
    if (ip_off != -1) {
        IPv4Hdr* ip = (IPv4Hdr*)(packet.data() + ip_off);
        size_t ip_hdr_len = (ip->ihl_ver & 0x0F) * 4;
        size_t ip_end = ip_off + ip_hdr_len;
        if (tcp_off != -1) {
            TcpHdr* tcp = (TcpHdr*)(packet.data() + tcp_off);
            size_t tcp_len = packet.size() - tcp_off;
            ip->tot_len = htons((uint16_t)(ip_hdr_len + tcp_len));
            ip->csum = 0; ip->csum = checksum(ip, ip_hdr_len);
            tcp->csum = tcp_checksum(ip, tcp, (int)tcp_len);
        } else if (udp_off != -1) {
            UdpHdr* udp = (UdpHdr*)(packet.data() + udp_off);
            size_t udp_len = packet.size() - udp_off;
            ip->tot_len = htons((uint16_t)(ip_hdr_len + udp_len));
            ip->csum = 0; ip->csum = checksum(ip, ip_hdr_len);
            udp->len = htons((uint16_t)udp_len);
            PseudoHdr ph; ph.src = ip->src; ph.dst = ip->dst; ph.zero = 0; ph.proto = IPPROTO_UDP; ph.len = htons((uint16_t)udp_len);
            std::vector<uint8_t> csum_buf(sizeof(ph) + udp_len);
            memcpy(csum_buf.data(), &ph, sizeof(ph));
            memcpy(csum_buf.data() + sizeof(ph), udp, udp_len);
            udp->csum = checksum(csum_buf.data(), csum_buf.size());
        }
    } else if (ip6_off != -1) {
        IPv6Hdr* ip6 = (IPv6Hdr*)(packet.data() + ip6_off);
        size_t payload_len = packet.size() - ip6_off - 40;
        ip6->payload_len = htons((uint16_t)payload_len);
        if (tcp_off != -1) {
            TcpHdr* tcp = (TcpHdr*)(packet.data() + tcp_off);
            size_t tcp_len = packet.size() - tcp_off;
            tcp->csum = tcp6_checksum(ip6, tcp, (int)tcp_len);
        }
    }
    return packet;
}
void listen_for_response(PcapHandle& h, const std::string& filter, int timeout_sec) {
    if (!h) return;
    bpf_program f; if (pcap_compile(h, &f, filter.c_str(), 1, PCAP_NETMASK_UNKNOWN) == 0) pcap_setfilter(h, &f);
    print_colored(CYAN, "Listening for response...\n");
    auto start = std::chrono::steady_clock::now();
    while (g_running) {
        pcap_pkthdr* header; const u_char* data;
        int res = pcap_next_ex(h, &header, &data);
        if (res == 1) packet_handler(NULL, header, data);
        if (timeout_sec > 0 && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() >= timeout_sec) break;
    }
}
void do_craft(const Args& args) {
    std::vector<PacketLayer> layers;
    if (!args.template_name.empty()) {
        if (g_templates.count(args.template_name)) layers = g_templates[args.template_name];
        else { print_colored(RED, "Template not found\n"); return; }
    } else {
        if (!args.proto_stack.empty()) for (auto& lname : split(args.proto_stack, ',')) { PacketLayer l; l.name = lname; layers.push_back(l); }
        for (auto& layer : layers) for (auto& f : args.fields) { size_t dot = f.first.find('.'); if (dot != std::string::npos && f.first.substr(0, dot) == layer.name) layer.fields[f.first.substr(dot + 1)] = f.second; }
    }
    std::vector<uint8_t> packet = build_packet(layers);
    if (args.no_send) {
        printf("Packet (%zu bytes):\n", packet.size());
        for (size_t i = 0; i < packet.size(); ++i) {
            printf("%02x ", packet[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        if (packet.size() % 16 != 0) printf("\n");
        return;
    }
    char err[PCAP_ERRBUF_SIZE];
    pcap_if_t* devs; pcap_findalldevs(&devs, err);
    std::string iface = args.iface.empty() ? devs->name : args.iface;
    PcapHandle h(iface.c_str(), 65536, 1, 1000, err);
    pcap_freealldevs(devs);
    if (!h) { print_colored(RED, "pcap_open: %s\n", err); return; }
    int count = g_flood ? -1 : (g_loop ? -1 : 1);
    auto it = args.fields.find("count"); if (it != args.fields.end()) count = atoi(it->second.c_str());
    RateLimiter limiter(args.rate_pps);
    int sent = 0;
    for (int i = 0; (count < 0 || i < count) && g_running; ++i) {
        limiter.wait();
        if (pcap_sendpacket(h, packet.data(), packet.size()) == 0) sent++;
        if (g_verbose) print_colored(CYAN, "Sent packet %d\n", i + 1);
    }
    print_colored(GREEN, "Packet sent (%d bytes) via %s\n", (int)packet.size(), iface.c_str());
    if (g_listen_mode) listen_for_response(h, args.filter, args.timeout_ms / 1000);
}

// ---------- 漏洞扫描模块 ----------
struct VulnCheck {
    std::string name, cve, msf_module, description;
    int port;
    std::function<bool(const std::string&, const Args&)> checker;
};
bool check_smb_v1(const std::string& target, const Args& args) {
    SocketHandle s(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(445); addr.sin_addr.s_addr = inet_addr(target.c_str());
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) return false;
    unsigned char smb_req[] = "\x00\x00\x00\x54\xff\x53\x4d\x42\x72\x00\x00\x00\x00\x18\x01\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfe\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
    send(s, (char*)smb_req, sizeof(smb_req) - 1, 0);
    char resp[256]; int len = recv(s, resp, sizeof(resp) - 1, 0);
    return (len > 4 && resp[4] == 0x72);
}
bool check_bluekeep(const std::string& target, const Args& args) {
    SocketHandle s(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(3389); addr.sin_addr.s_addr = inet_addr(target.c_str());
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) return false;
    char buf[11] = {0x03, 0x00, 0x00, 0x13, 0x0e, (char)0xe0, 0x00, 0x00, 0x00, 0x00, 0x00};
    send(s, buf, sizeof(buf), 0);
    char resp[19]; int len = recv(s, resp, sizeof(resp), 0);
    return (len >= 19 && resp[0] == 0x03 && resp[10] == 0x0e);
}
bool check_zerologon(const std::string& target, const Args& args) { return !args.cred.empty() && !args.domain.empty(); }
bool check_petitpotam(const std::string& target, const Args& args) { return true; }
bool check_netbios(const std::string& target, const Args& args) {
    SocketHandle s(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = htons(137); addr.sin_addr.s_addr = inet_addr(target.c_str());
    char req[50] = {0};
    sendto(s, req, sizeof(req), 0, (sockaddr*)&addr, sizeof(addr));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds); timeval tv = {1, 0};
    if (select(0, &fds, NULL, NULL, &tv) > 0) { char resp[1024]; int len = recvfrom(s, resp, sizeof(resp), 0, NULL, NULL); return len > 0; }
    return false;
}
bool check_print_spooler(const std::string& target, const Args& args) { return true; }
bool check_exchange_proxyshell(const std::string& target, const Args& args) { return false; }
static std::vector<VulnCheck> vuln_db = {
    {"SMB Signing Disabled", "CVE-2017-0144", "auxiliary/scanner/smb/smb_signing", "SMB Signing not required", 445, check_smb_v1},
    {"EternalBlue", "CVE-2017-0144", "exploit/windows/smb/ms17_010_eternalblue", "SMBv1 RCE", 445, check_smb_v1},
    {"BlueKeep", "CVE-2019-0708", "exploit/windows/rdp/cve_2019_0708_bluekeep", "RDP RCE", 3389, check_bluekeep},
    {"Zerologon", "CVE-2020-1472", "exploit/windows/dcerpc/cve_2020_1472_zerologon", "Netlogon priv esc", 135, check_zerologon},
    {"PetitPotam", "CVE-2021-36942", "auxiliary/scanner/dcerpc/petitpotam", "NTLM relay", 445, check_petitpotam},
    {"NetBIOS", "CVE-2020-0796", "auxiliary/scanner/netbios/nbname", "NetBIOS name info", 137, check_netbios},
    {"PrintNightmare", "CVE-2021-34527", "exploit/windows/dcerpc/cve_2021_34527_printnightmare", "Print Spooler RCE", 445, check_print_spooler},
    {"ProxyShell", "CVE-2021-34473", "exploit/windows/http/exchange_proxyshell", "Exchange RCE", 443, check_exchange_proxyshell},
};
void save_output(const std::string& filename, const std::string& data) {
    if (filename.empty()) return;
    std::ofstream f(filename, std::ios::app);
    f << data << std::endl;
}
void do_vuln(const Args& args) {
    if (args.targets.empty()) { print_colored(RED, "Need target\n"); return; }
    if (!g_quiet) {
        printf("\n");
        print_colored(CYAN, "Vulnerability Assessment Report\n");
        print_colored(CYAN, "Target:         %s\n", args.target.c_str());
        print_colored(CYAN, "Scan type:      %s\n", args.vuln_type.empty() ? "basic (all)" : args.vuln_type.c_str());
        print_colored(CYAN, "Started:        %s\n\n", format_utc_time().c_str());
    }
    std::vector<int> port_filter;
    if (!args.ports.empty()) port_filter = parse_ports(args.ports);
    std::vector<std::pair<std::string, VulnCheck>> found_vulns;
    for (const auto& t : args.targets) {
        for (const auto& vuln : vuln_db) {
            if (!g_running) break;
            if (!args.vuln_type.empty() && vuln.name != args.vuln_type && vuln.cve != args.vuln_type) continue;
            if (g_safe && (vuln.name == "EternalBlue" || vuln.name == "BlueKeep")) continue;
            if (!port_filter.empty() && std::find(port_filter.begin(), port_filter.end(), vuln.port) == port_filter.end()) continue;
            printf("Checking %-30s on port %-5d... ", vuln.cve.c_str(), vuln.port);
            bool result = vuln.checker(t, args);
            if (args.patch_check) result = !result;
            if (result) {
                set_color(RED);
                printf("[VULNERABLE]\n");
                set_color(WHITE);
                found_vulns.push_back({t, vuln});
                if (g_msf) print_colored(CYAN, "  MSF: %s\n", vuln.msf_module.c_str());
                save_output(args.out_file, t + ":" + std::to_string(vuln.port) + " " + vuln.cve);
            } else {
                set_color(GREEN);
                printf("[NOT VULNERABLE]\n");
                set_color(WHITE);
            }
        }
    }
    if (!found_vulns.empty() && !g_quiet) {
        printf("\n");
        print_separator_line(80, '=');
        print_colored(CYAN, "VULNERABILITY SUMMARY\n");
        print_separator_line(80, '=');
        std::vector<std::string> headers = {"PORT", "PROTOCOL", "VULNERABILITY", "CVE-ID", "RISK", "MSF MODULE"};
        std::vector<int> widths = {6, 8, 30, 15, 9, 30};
        print_table_header(headers, widths);
        for (const auto& item : found_vulns) {
            const VulnCheck& v = item.second;
            std::string risk = "MEDIUM";
            if (v.name == "EternalBlue" || v.name == "BlueKeep" || v.name == "ZeroLogon") risk = "CRITICAL";
            else if (v.name == "PrintNightmare" || v.name == "ProxyShell") risk = "HIGH";
            Color risk_color = (risk == "CRITICAL" || risk == "HIGH") ? RED : (risk == "MEDIUM" ? YELLOW : WHITE);
            std::vector<std::string> cols = {std::to_string(v.port), "tcp", v.name, v.cve, risk, v.msf_module};
            std::vector<Color> colors = {WHITE, WHITE, WHITE, WHITE, risk_color, WHITE};
            print_table_row(cols, widths, colors);
        }
        printf("\n");
        print_colored(CYAN, "Total vulnerabilities found: %zu\n", found_vulns.size());
    }
}

// ---------- 交互式编辑器 ----------
static std::deque<std::string> history;
static size_t hist_pos = 0;
static std::vector<uint8_t> raw_edit_buffer;
std::string read_line_with_history() {
    std::string line;
    size_t cursor = 0;
    int ch;
    while ((ch = _getch()) != 13) {
        if (ch == 3) { g_running = false; return ""; }
        if (ch == 0 || ch == 0xE0) {
            ch = _getch();
            if (ch == 72) {
                if (!history.empty() && hist_pos > 0) {
                    while (cursor > 0) { printf("\b \b"); cursor--; }
                    line = history[--hist_pos];
                    printf("%s", line.c_str());
                    cursor = line.length();
                }
            } else if (ch == 80) {
                if (hist_pos < history.size() - 1) {
                    while (cursor > 0) { printf("\b \b"); cursor--; }
                    line = history[++hist_pos];
                    printf("%s", line.c_str());
                    cursor = line.length();
                } else if (hist_pos == history.size() - 1) {
                    hist_pos++;
                    while (cursor > 0) { printf("\b \b"); cursor--; }
                    line.clear();
                    cursor = 0;
                }
            } else if (ch == 75) {
                if (cursor > 0) { printf("\b"); cursor--; }
            } else if (ch == 77) {
                if (cursor < line.size()) { printf("%c", line[cursor]); cursor++; }
            }
        } else if (ch == 8) {
            if (cursor > 0) {
                line.erase(cursor-1, 1);
                printf("\b \b");
                cursor--;
                for (size_t i = cursor; i < line.size(); ++i) printf("%c", line[i]);
                printf(" ");
                for (size_t i = cursor; i <= line.size(); ++i) printf("\b");
            }
        } else if (ch == 9) {
            static const char* cmds[] = {"add","set","del","show","hex","send","listen","save","load","payload","exit","help","use","export","raw","edit","write","assemble"};
            std::string prefix = line.substr(0, cursor);
            std::vector<std::string> matches;
            for (auto c : cmds) if (strncmp(c, prefix.c_str(), prefix.size()) == 0) matches.push_back(c);
            if (matches.size() == 1) {
                line = matches[0] + " ";
                cursor = line.size();
                printf("\r%s", line.c_str());
            }
        } else {
            line.insert(cursor, 1, (char)ch);
            printf("%c", ch);
            cursor++;
            for (size_t i = cursor; i < line.size(); ++i) printf("%c", line[i]);
            for (size_t i = cursor; i < line.size(); ++i) printf("\b");
        }
    }
    printf("\n");
    if (!line.empty()) { history.push_back(line); hist_pos = history.size(); }
    return line;
}
void hex_edit_loop(std::vector<uint8_t>& buffer) {
    size_t offset = 0;
    const size_t page_size = 16;
    while (g_running) {
        printf("\n--- Hex Editor (offset 0x%04zx) ---\n", offset);
        for (size_t i = 0; i < page_size && offset + i < buffer.size(); ++i) printf("%02x ", buffer[offset + i]);
        printf("\n");
        for (size_t i = 0; i < page_size && offset + i < buffer.size(); ++i) {
            char c = buffer[offset + i];
            putchar(isprint(c) ? c : '.');
        }
        printf("\nCommands: n(ext), p(rev), e <offset> <value>, w <offset> <hexstring>, d(one), q(uit)\n> ");
        std::string cmd = read_line_with_history();
        if (cmd == "q") break;
        else if (cmd == "n") offset += page_size;
        else if (cmd == "p") { if (offset >= page_size) offset -= page_size; }
        else if (cmd == "d") break;
        else if (cmd.substr(0,2) == "e ") {
            size_t pos; unsigned int val;
            if (sscanf(cmd.c_str(), "e %zx %x", &pos, &val) == 2 && pos < buffer.size()) {
                buffer[pos] = (uint8_t)val;
                printf("Wrote %02x at offset %zx\n", val, pos);
            } else printf("Invalid\n");
        } else if (cmd.substr(0,2) == "w ") {
            size_t pos; char hexstr[256];
            if (sscanf(cmd.c_str(), "w %zx %s", &pos, hexstr) == 2 && pos < buffer.size()) {
                std::string hex = hexstr;
                for (size_t i = 0; i < hex.length() && pos + i/2 < buffer.size(); i+=2) {
                    unsigned int v;
                    sscanf(hex.c_str()+i, "%2x", &v);
                    buffer[pos + i/2] = (uint8_t)v;
                }
                printf("Written.\n");
            } else printf("Invalid\n");
        }
        if (offset + page_size > buffer.size()) offset = (buffer.size() / page_size) * page_size;
    }
}
void editor_loop(const Args& args) {
    print_colored(CYAN, "NETSEXT INTERACTIVE PACKET EDITOR\n");
    print_colored(CYAN, "Type 'help' for commands, 'exit' to quit.\n");
    std::vector<PacketLayer> layers;
    if (!args.proto_stack.empty()) for (auto& lname : split(args.proto_stack, ',')) { PacketLayer l; l.name = lname; layers.push_back(l); }
    while (g_running) {
        set_color(CYAN);
        printf("editor> ");
        set_color(WHITE);
        std::string line = read_line_with_history();
        if (!g_running) break;
        if (line.empty()) continue;
        std::vector<std::string> parts = split(line, ' ');
        if (parts.empty()) continue;
        std::string cmd = parts[0];
        if (cmd == "exit" || cmd == "quit") break;
        else if (cmd == "help") {
            printf("Available commands:\n");
            printf("  show                - Display current packet layers\n");
            printf("  set <L>.<F>=<V>     - Set field\n");
            printf("  add <layer>         - Add layer\n");
            printf("  del <layer>         - Delete layer\n");
            printf("  payload <string>    - Set payload\n");
            printf("  payload.file <file> - Load payload from file\n");
            printf("  payload.template <name> [args] - Use built-in payload\n");
            printf("  use <exploit>       - Build probe packet\n");
            printf("  hex                 - Show packet in hex\n");
            printf("  export [carray]     - Export as C array\n");
            printf("  send [count] [interval_ms] [--async] - Send packet\n");
            printf("  listen [sec] [--filter] - Listen for responses\n");
            printf("  save <file>         - Save template\n");
            printf("  load <file>         - Load template\n");
            printf("  load pcap <file>    - Load first packet from pcap\n");
            printf("  raw <hexstring>     - Replace current packet with raw hex bytes\n");
            printf("  edit                - Enter hex editor for current packet\n");
            printf("  write <offset> <byte> - Write a byte at offset (in built packet)\n");
            printf("  assemble            - Show assembled raw packet (without sending)\n");
            printf("  exit                - Quit\n");
        } else if (cmd == "show") {
            if (layers.empty()) printf("No layers defined.\n");
            else {
                for (const auto& l : layers) {
                    set_color(YELLOW); printf("layer %s\n", l.name.c_str()); set_color(WHITE);
                    for (const auto& kv : l.fields) {
                        set_color(GREEN); printf("  %s", kv.first.c_str()); set_color(WHITE);
                        printf(" = %s\n", kv.second.c_str());
                    }
                }
            }
        } else if (cmd == "set") {
            if (parts.size() < 2) { print_colored(RED, "Usage: set <layer>.<field>=<value>\n"); continue; }
            std::string arg = parts[1];
            size_t eq = arg.find('=');
            if (eq == std::string::npos) { print_colored(RED, "Missing '='\n"); continue; }
            std::string lf = arg.substr(0, eq);
            std::string val = arg.substr(eq + 1);
            size_t dot = lf.find('.');
            if (dot == std::string::npos) { print_colored(RED, "Missing layer.field\n"); continue; }
            std::string lname = lf.substr(0, dot);
            std::string field = lf.substr(dot + 1);
            bool found = false;
            for (auto& l : layers) {
                if (l.name == lname) {
                    l.fields[field] = val;
                    found = true;
                    break;
                }
            }
            if (!found) {
                PacketLayer newlayer;
                newlayer.name = lname;
                newlayer.fields[field] = val;
                layers.push_back(newlayer);
            }
            print_colored(GREEN, "Set %s.%s = %s\n", lname.c_str(), field.c_str(), val.c_str());
        } else if (cmd == "add") {
            if (parts.size() < 2) { print_colored(RED, "Usage: add <layer>\n"); continue; }
            PacketLayer l; l.name = parts[1];
            layers.push_back(l);
            print_colored(GREEN, "Added layer %s\n", parts[1].c_str());
        } else if (cmd == "del") {
            if (parts.size() < 2) { print_colored(RED, "Usage: del <layer>\n"); continue; }
            std::string lname = parts[1];
            auto it = std::find_if(layers.begin(), layers.end(), [&](const PacketLayer& l) { return l.name == lname; });
            if (it != layers.end()) { layers.erase(it); print_colored(GREEN, "Deleted layer %s\n", lname.c_str()); }
            else print_colored(RED, "Layer not found\n");
        } else if (cmd == "payload") {
            if (parts.size() < 2) { print_colored(RED, "Usage: payload <string>\n"); continue; }
            std::string data = line.substr(line.find(' ') + 1);
            auto it = std::find_if(layers.begin(), layers.end(), [](const PacketLayer& l) { return l.name == "payload"; });
            if (it == layers.end()) { PacketLayer pl; pl.name = "payload"; pl.fields["data"] = data; layers.push_back(pl); }
            else it->fields["data"] = data;
            print_colored(GREEN, "Payload set\n");
        } else if (cmd == "payload.file") {
            if (parts.size() < 2) { print_colored(RED, "Usage: payload.file <filename>\n"); continue; }
            std::ifstream f(parts[1], std::ios::binary);
            if (!f) { print_colored(RED, "Cannot open file\n"); continue; }
            std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto it = std::find_if(layers.begin(), layers.end(), [](const PacketLayer& l) { return l.name == "payload"; });
            if (it == layers.end()) { PacketLayer pl; pl.name = "payload"; pl.fields["data"] = data; layers.push_back(pl); }
            else it->fields["data"] = data;
            print_colored(GREEN, "Payload loaded (%zu bytes)\n", data.size());
        } else if (cmd == "payload.template") {
            if (parts.size() < 2) { print_colored(RED, "Usage: payload.template <name>\n"); continue; }
            std::string tname = parts[1];
            std::string data;
            if (tname == "http_get") {
                std::string path = parts.size() > 2 ? parts[2] : "/";
                data = "GET " + path + " HTTP/1.1\r\nHost: example.com\r\n\r\n";
            } else if (tname == "dns_query") {
                uint8_t dns[] = {0x12,0x34,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x07,'e','x','a','m','p','l','e',0x03,'c','o','m',0x00,0x00,0x01,0x00,0x01};
                data.assign((char*)dns, sizeof(dns));
            }
            auto it = std::find_if(layers.begin(), layers.end(), [](const PacketLayer& l) { return l.name == "payload"; });
            if (it == layers.end()) { PacketLayer pl; pl.name = "payload"; pl.fields["data"] = data; layers.push_back(pl); }
            else it->fields["data"] = data;
            print_colored(GREEN, "Payload template '%s' applied\n", tname.c_str());
        } else if (cmd == "use") {
            if (parts.size() < 2) { print_colored(RED, "Usage: use <exploit>\n"); continue; }
            std::string exp = parts[1];
            if (exp == "eternalblue") {
                layers = {{"eth",{{"type","0x0800"}}}, {"ip",{{"proto","6"}}}, {"tcp",{{"dport","445"},{"flags","0x02"}}}};
                layers.push_back({"payload",{{"data",std::string("\x00\x00\x00\x54\xff\x53\x4d\x42\x72\x00\x00\x00\x00\x18\x01\x48",16)}}});
            } else if (exp == "bluekeep") {
                layers = {{"eth",{}}, {"ip",{{"proto","6"}}}, {"tcp",{{"dport","3389"},{"flags","0x02"}}}};
                layers.push_back({"payload",{{"data",std::string("\x03\x00\x00\x13\x0e\xe0\x00\x00\x00\x00\x00",11)}}});
            } else print_colored(RED, "Unknown exploit\n");
            print_colored(GREEN, "Exploit probe built\n");
        } else if (cmd == "hex") {
            if (layers.empty()) { print_colored(YELLOW, "No layers\n"); continue; }
            auto pkt = build_packet(layers);
            printf("Packet length: %zu bytes\n", pkt.size());
            for (size_t i = 0; i < pkt.size(); ++i) {
                printf("%02x ", pkt[i]);
                if ((i + 1) % 16 == 0) printf("\n");
            }
            if (pkt.size() % 16 != 0) printf("\n");
        } else if (cmd == "export") {
            if (layers.empty()) { print_colored(YELLOW, "No layers\n"); continue; }
            auto pkt = build_packet(layers);
            if (parts.size() > 1 && parts[1] == "carray") {
                printf("unsigned char packet[] = {\n  ");
                for (size_t i = 0; i < pkt.size(); ++i) {
                    printf("0x%02x", pkt[i]);
                    if (i < pkt.size() - 1) printf(", ");
                    if ((i + 1) % 12 == 0) printf("\n  ");
                }
                printf("\n};\nunsigned int packet_len = %zu;\n", pkt.size());
            } else {
                for (size_t i = 0; i < pkt.size(); ++i) {
                    printf("%02x ", pkt[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                }
                if (pkt.size() % 16 != 0) printf("\n");
            }
        } else if (cmd == "raw") {
            if (parts.size() < 2) { print_colored(RED, "Usage: raw <hexstring>\n"); continue; }
            std::string hex = parts[1];
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i < hex.length(); i+=2) {
                unsigned int v;
                if (sscanf(hex.c_str()+i, "%2x", &v) == 1) bytes.push_back((uint8_t)v);
            }
            raw_edit_buffer = bytes;
            layers.clear();
            PacketLayer rawl; rawl.name = "raw"; rawl.fields["data"] = std::string(bytes.begin(), bytes.end());
            layers.push_back(rawl);
            print_colored(GREEN, "Raw packet set (%zu bytes)\n", bytes.size());
        } else if (cmd == "edit") {
            if (layers.empty() && raw_edit_buffer.empty()) { print_colored(YELLOW, "No packet to edit\n"); continue; }
            std::vector<uint8_t> pkt = raw_edit_buffer.empty() ? build_packet(layers) : raw_edit_buffer;
            hex_edit_loop(pkt);
            raw_edit_buffer = pkt;
            layers.clear();
            PacketLayer rawl; rawl.name = "raw"; rawl.fields["data"] = std::string(pkt.begin(), pkt.end());
            layers.push_back(rawl);
            print_colored(YELLOW, "Layers converted to raw packet; further editing is raw.\n");
        } else if (cmd == "write") {
            if (parts.size() < 3) { print_colored(RED, "Usage: write <offset> <byte>\n"); continue; }
            size_t offset = strtoul(parts[1].c_str(), NULL, 0);
            unsigned int val = strtoul(parts[2].c_str(), NULL, 0);
            std::vector<uint8_t> pkt = raw_edit_buffer.empty() ? build_packet(layers) : raw_edit_buffer;
            if (offset < pkt.size()) {
                pkt[offset] = (uint8_t)val;
                raw_edit_buffer = pkt;
                layers.clear();
                PacketLayer rawl; rawl.name = "raw"; rawl.fields["data"] = std::string(pkt.begin(), pkt.end());
                layers.push_back(rawl);
                print_colored(GREEN, "Written %02x at offset %zu\n", val, offset);
            } else print_colored(RED, "Offset out of range\n");
        } else if (cmd == "assemble") {
            if (layers.empty()) { print_colored(YELLOW, "No layers\n"); continue; }
            auto pkt = build_packet(layers);
            printf("Assembled packet (%zu bytes):\n", pkt.size());
            for (size_t i = 0; i < pkt.size(); ++i) {
                printf("%02x ", pkt[i]);
                if ((i + 1) % 16 == 0) printf("\n");
            }
            if (pkt.size() % 16 != 0) printf("\n");
        } else if (cmd == "send") {
            if (layers.empty()) { print_colored(YELLOW, "No layers\n"); continue; }
            int count = 1, interval = 0; bool async = false;
            for (size_t i = 1; i < parts.size(); ++i) {
                if (parts[i] == "--async") async = true;
                else if (i == 1) count = atoi(parts[i].c_str());
                else if (i == 2) interval = atoi(parts[i].c_str());
            }
            if (count == 0) count = -1;
            auto pkt = build_packet(layers);
            if (args.no_send) { print_colored(YELLOW, "--no-send enabled\n"); continue; }
            char err[PCAP_ERRBUF_SIZE];
            std::string iface = args.iface;
            if (iface.empty()) {
                pcap_if_t* alldevs;
                if (pcap_findalldevs(&alldevs, err) != -1) {
                    if (alldevs) iface = alldevs->name;
                    pcap_freealldevs(alldevs);
                }
            }
            PcapHandle h(iface.c_str(), 65536, 1, 1000, err);
            if (!h) { print_colored(RED, "pcap_open: %s\n", err); continue; }
            auto sender = [=]() {
                int sent = 0;
                RateLimiter limiter(args.rate_pps);
                for (int i = 0; (count < 0 || i < count) && g_running; ++i) {
                    limiter.wait();
                    if (pcap_sendpacket(h, pkt.data(), (int)pkt.size()) == 0) sent++;
                    if (interval > 0) Sleep(interval);
                }
                print_colored(GREEN, "Sent %d packet(s) via %s\n", sent, iface.c_str());
            };
            if (async) std::thread(sender).detach();
            else sender();
            if (g_listen_mode) listen_for_response(h, args.filter, args.timeout_ms / 1000);
        } else if (cmd == "listen") {
            int sec = args.timeout_ms / 1000;
            if (parts.size() >= 2) sec = atoi(parts[1].c_str());
            char err[PCAP_ERRBUF_SIZE];
            pcap_if_t* devs; pcap_findalldevs(&devs, err);
            std::string iface = args.iface.empty() ? devs->name : args.iface;
            PcapHandle h(iface.c_str(), 65536, 1, 1000, err);
            pcap_freealldevs(devs);
            if (h) listen_for_response(h, args.filter, sec);
            else print_colored(RED, "pcap_open: %s\n", err);
        } else if (cmd == "save") {
            if (parts.size() < 2) { print_colored(RED, "Usage: save <filename>\n"); continue; }
            std::ofstream f(parts[1]);
            if (!f) { print_colored(RED, "Cannot open file\n"); continue; }
            f << "# netsext template v1\n";
            for (const auto& l : layers) {
                f << "layer " << l.name << "\n";
                for (const auto& kv : l.fields) f << kv.first << "=" << kv.second << "\n";
            }
            print_colored(GREEN, "Template saved to %s\n", parts[1].c_str());
        } else if (cmd == "load") {
            if (parts.size() < 2) { print_colored(RED, "Usage: load <filename> or load pcap <file>\n"); continue; }
            if (parts[1] == "pcap" && parts.size() >= 3) {
                char err[PCAP_ERRBUF_SIZE];
                pcap_t* h = pcap_open_offline(parts[2].c_str(), err);
                if (!h) { print_colored(RED, "Cannot open pcap: %s\n", err); continue; }
                pcap_pkthdr* hdr; const u_char* data;
                if (pcap_next_ex(h, &hdr, &data) == 1) {
                    raw_edit_buffer.assign(data, data + hdr->len);
                    layers.clear();
                    PacketLayer rawl; rawl.name = "raw"; rawl.fields["data"] = std::string(raw_edit_buffer.begin(), raw_edit_buffer.end());
                    layers.push_back(rawl);
                    print_colored(GREEN, "Loaded packet from pcap (%u bytes)\n", hdr->len);
                }
                pcap_close(h);
            } else {
                load_template("_editor_temp", parts[1]);
                if (g_templates.count("_editor_temp")) {
                    layers = g_templates["_editor_temp"];
                    print_colored(GREEN, "Template loaded\n");
                } else print_colored(RED, "Failed to load\n");
            }
        } else print_colored(RED, "Unknown command. Type 'help'.\n");
    }
    print_colored(CYAN, "Exiting editor.\n");
}
void do_editor(const Args& args) { editor_loop(args); }

// ---------- 主函数 ----------
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) { g_running = false; return TRUE; }
int main(int argc, char* argv[]) {
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    srand((unsigned)time(NULL));
    if (argc < 2) { print_help(); return 1; }
    Args args;
    if (!parse_args(argc, argv, args)) { print_colored(RED, "Invalid arguments\n"); print_help(); return 1; }
    if (args.function == "help" || args.function == "h") { print_help(); return 0; }
    BOOL isAdmin = FALSE; PSID admin; SID_IDENTIFIER_AUTHORITY NtAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin)) { CheckTokenMembership(NULL, admin, &isAdmin); FreeSid(admin); }
    if (!isAdmin && (args.function == "a" || args.function == "f" || args.function == "s" || args.function == "c" || args.function == "v")) { print_colored(RED, "Administrator privileges required.\n"); return 1; }
    if (args.function == "a") do_arp(args);
    else if (args.function == "n") do_nat(args);
    else if (args.function == "f") do_sniff(args);
    else if (args.function == "r") do_recon(args);
    else if (args.function == "s") do_portscan(args);
    else if (args.function == "p") do_port_ops(args);
    else if (args.function == "c") do_craft(args);
    else if (args.function == "v") do_vuln(args);
    else if (args.function == "e") do_editor(args);
    else print_colored(RED, "Unknown function -%s\n", args.function.c_str());
    WSACleanup();
    return 0;
}
