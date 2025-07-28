#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>

#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

using namespace std;

// Struktura dlya seyva proxy
struct ProxyServer {
    wstring address;
    int port;
    bool isWorking;
};

// Razborka strok proxy
ProxyServer parseProxyLine(const wstring& line) {
    ProxyServer proxy = { L"", 0, false };

    // Пропускаем пустые строки
    if (line.empty()) return proxy;

    size_t delimiter_pos = line.find(L':');
    if (delimiter_pos == wstring::npos) {
        wcerr << L"Invalid proxy format: " << line << endl;
        return proxy;
    }

    //Proverka proxy:port func 
    proxy.address = line.substr(0, delimiter_pos);
    try {
        proxy.port = stoi(line.substr(delimiter_pos + 1));
    }
    catch (...) {
        wcerr << L"Invalid port number in: " << line << endl;
        proxy.port = 0;
    }

    return proxy;
}

// Proxy check func
bool checkProxy(const ProxyServer& proxy) {
    if (proxy.address.empty() || proxy.port <= 0) {
        return false;
    }

    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in proxyAddr = { 0 };

    // Winsock initializ
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        wcerr << L"WSAStartup failed" << endl;
        return false;
    }

    // Socket creation
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        wcerr << L"Socket creation failed" << endl;
        WSACleanup();
        return false;
    }

    //proxy adress settings
    proxyAddr.sin_family = AF_INET;
    proxyAddr.sin_port = htons(proxy.port);
    if (InetPton(AF_INET, proxy.address.c_str(), &proxyAddr.sin_addr) <= 0) {
        wcerr << L"Invalid proxy address" << endl;
        closesocket(sock);
        WSACleanup();
        return false;
    }


    DWORD timeout = 3000; // 3 секунды
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    //retry method
    if (connect(sock, (sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    closesocket(sock);
    WSACleanup();
    return true;
}

// winsys proxy setup idk
bool setSystemProxy(const wstring& address, int port) {
    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION options[3];
    wstring proxyStr = address + L":" + to_wstring(port);
    //белый лист
    //wstring bypass = L"localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*";
 
    // vulue settings
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    options[0].Value.dwValue = PROXY_TYPE_PROXY;

    options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    options[1].Value.pszValue = (LPWSTR)proxyStr.c_str();

    options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
 //   options[2].Value.pszValue = (LPWSTR)bypass.c_str();

    list.dwSize = sizeof(list);
    list.pszConnection = NULL;
    list.dwOptionCount = 3;
    list.dwOptionError = 0;
    list.pOptions = options;

    if (!InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, sizeof(list))) {
        wcerr << L"Failed to set proxy options" << endl;
        return false;
    }

    // save 
    InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    return true;
}

// SYS proxy DISABLE
/* если закрыть .exe, прокси так и останется сохраненным выше :/
           нужно будет в ручную отключать прокси в системе.
*/

bool disableSystemProxy() {
    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION option;

    option.dwOption = INTERNET_PER_CONN_FLAGS;
    option.Value.dwValue = PROXY_TYPE_DIRECT;

    list.dwSize = sizeof(list);
    list.pszConnection = NULL;
    list.dwOptionCount = 1;
    list.dwOptionError = 0;
    list.pOptions = &option;

    if (!InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, sizeof(list))) {
        wcerr << L"Failed to disable proxy" << endl;
        return false;
    }

    InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    return true;
}

vector<ProxyServer> loadAndCheckProxies(const wstring& filename) {
    vector<ProxyServer> proxies;
    wifstream file(filename);

    if (!file.is_open()) {
        wcerr << L"Error: Could not open file " << filename << endl;
        return proxies;
    }

    wstring line;
    while (getline(file, line)) {
        if (!line.empty()) {
            ProxyServer proxy = parseProxyLine(line);
            if (!proxy.address.empty() && proxy.port > 0) {
                wcout << L"Checking proxy: " << proxy.address << L":" << proxy.port << L"... ";
                proxy.isWorking = checkProxy(proxy);
                wcout << (proxy.isWorking ? L"WORKING" : L"NOT WORKING") << endl;
                proxies.push_back(proxy);
            }
        }
    }

    file.close();
    return proxies;
}

// menu
void displayMenu(const vector<ProxyServer>& proxies) {
    wcout << L"\nAvailable proxy servers:\n";
    for (size_t i = 0; i < proxies.size(); i++) {
        wcout << i + 1 << L". " << proxies[i].address << L":" << proxies[i].port;
        wcout << (proxies[i].isWorking ? L" [WORKING]" : L" [NOT WORKING]") << endl;
    }

    wcout << L"\nOptions:\n";
    wcout << L"1-" << proxies.size() << L" - Select working proxy\n";
    wcout << L"0 - Disable proxy\n";
    wcout << L"q - Quit\n";
}

int main() {
    // force Unicode
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    wcout << L"Proxy Manager v1.2\n";
    wcout << L"Loading proxies from proxies.txt...\n";

    //setup proxy
    wstring filename = L"proxies.txt";
    vector<ProxyServer> proxies = loadAndCheckProxies(filename);

    if (proxies.empty()) {
        wcout << L"No valid proxies found. Please check your proxies.txt file.\n";
        wcout << L"Press Enter to exit...";
        cin.get();
        return 1;
    }

    while (true) {
        displayMenu(proxies);

        wstring input;
        wcout << L"\nSelect option: ";
        wcin >> input;

        if (input == L"q" || input == L"Q") {
            break;
        }

        if (input == L"0") {
            if (disableSystemProxy()) {
                wcout << L"Proxy disabled successfully\n";
            }
            else {
                wcerr << L"Failed to disable proxy\n";
            }
            continue;
        }

        try {
            int choice = stoi(input);
            if (choice >= 1 && choice <= proxies.size()) {
                ProxyServer& proxy = proxies[choice - 1];
                if (proxy.isWorking) {
                    if (setSystemProxy(proxy.address, proxy.port)) {
                        wcout << L"Proxy set successfully: " << proxy.address << L":" << proxy.port << endl;
                        wcout << L"Now you can use this proxy in your browser or any application\n";
                    }
                    else {
                        wcerr << L"Failed to set proxy\n";
                    }
                }
                else {
                    wcerr << L"Selected proxy is not working\n";
                }
            }
            else {
                wcerr << L"Invalid choice\n";
            }
        }
        catch (...) {
            wcerr << L"Invalid input\n";
        }
    }

    return 0;
}