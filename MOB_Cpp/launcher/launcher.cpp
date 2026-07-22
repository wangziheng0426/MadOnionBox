#include "launcher.h" // 包含 Launcher 类声明

#include <sensapi.h> // IsNetworkAlive：检测本机网络是否连通
#include <winhttp.h> // WinHTTP：HTTP 下载相关 API

#include <sstream> // 字符串流，用于按行解析、拆分版本号等
#include <vector>  // 动态数组，用于命令行缓冲、下载分块等

#pragma comment(lib, "winhttp.lib") // 链接 WinHTTP 库（MSVC 常用写法）
#pragma comment(lib, "sensapi.lib") // 链接 SensAPI 库（网络连通检测）

namespace { // 匿名命名空间：本文件内部辅助函数，外部不可见

// 检测本机是否有可用网络（局域网或广域网）；不通则无需发起下载
bool isNetworkAvailable()
{
    DWORD flags = 0;
    return IsNetworkAlive(&flags) != FALSE;
}

// 去掉字符串首尾空白（空格、制表符、换行）
std::string trim(const std::string &value)
{
    const auto begin = value.find_first_not_of(" \t\r\n"); // 第一个非空白位置
    if (begin == std::string::npos) { // 全是空白
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n"); // 最后一个非空白位置
    return value.substr(begin, end - begin + 1);        // 截取中间有效部分
}

// 从 update.txt 解析同名键的全部取值（多条 packageUrl）
bool extractIniValues(const std::string &text, const std::string &key, std::vector<std::string> &outValues)
{
    outValues.clear();
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '[' || line.front() == '#' || line.front() == ';') {
            continue;
        }
        const auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }
        if (trim(line.substr(0, eqPos)) != key) {
            continue;
        }
        const std::string value = trim(line.substr(eqPos + 1));
        if (!value.empty()) {
            outValues.push_back(value);
        }
    }
    return !outValues.empty();
}

// 从 update.txt（INI 风格 key=value）中解析指定键（取第一条）
bool extractIniValue(const std::string &text, const std::string &key, std::string &outValue)
{
    std::vector<std::string> values;
    if (!extractIniValues(text, key, values)) {
        return false;
    }
    outValue = values.front();
    return true;
}

// 将 UTF-8 的 std::string 转成 Windows 宽字符串 std::wstring
std::wstring utf8ToWide(const std::string &text)
{
    if (text.empty()) {
        return {}; // 空串直接返回
    }

    // 第一次调用：只计算需要的宽字符个数（含结尾 '\0'）
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {}; // 转换失败
    }

    std::wstring wide(static_cast<size_t>(size), L'\0'); // 按长度分配缓冲
    // 第二次调用：真正写入宽字符数据
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), size);
    // MultiByteToWideChar 会带上结尾 '\0'，从 wstring 里去掉多余的一个
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

// 检测远程服务器是否有响应；无响应时快速返回 false（HEAD，不下载正文）
bool canReachRemoteServer(const std::string &url)
{
    const std::wstring wideUrl = utf8ToWide(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);

    wchar_t hostName[256] = {0};
    wchar_t urlPath[2048] = {0};
    components.lpszHostName = hostName;
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostName));
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        return false;
    }

    HINTERNET session = WinHttpOpen(L"MadOnionBoxLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }

    // 解析/连接/发送/接收均 3 秒超时，服务器无响应时尽快失败
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);

    HINTERNET connection = WinHttpConnect(session, components.lpszHostName, components.nPort, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }

    const bool useHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
    const DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"HEAD", components.lpszUrlPath, nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    if (useHttps) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
    }

    const bool reachable =
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return reachable;
}

// 将系统本地编码（ACP，来自 GetModuleFileNameA 等）转成宽字符串，供 CreateProcessW 使用
std::wstring ansiToWide(const std::string &text)
{
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wide.data(), size);
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

// 隐藏窗口方式运行外部命令，并通过 exitCode 返回进程退出码
bool runHiddenProcess(const std::string &commandLine, DWORD &exitCode)
{
    STARTUPINFOW startupInfo{};           // 启动信息（窗口显示方式等）
    PROCESS_INFORMATION processInfo{};    // 进程/线程句柄等信息
    startupInfo.cb = sizeof(startupInfo); // 必须填结构体大小
    startupInfo.dwFlags = STARTF_USESHOWWINDOW; // 启用 wShowWindow 字段
    startupInfo.wShowWindow = SW_HIDE;    // 隐藏窗口

    // CreateProcessW 可能修改命令行缓冲，因此需要可变宽字符副本
    std::wstring wideCommand = ansiToWide(commandLine);
    std::vector<wchar_t> mutableCommand(wideCommand.begin(), wideCommand.end());
    mutableCommand.push_back(L'\0');

    // 创建子进程：不显示控制台窗口
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startupInfo, &processInfo)) {
        return false; // 创建失败
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE); // 等待进程结束
    GetExitCodeProcess(processInfo.hProcess, &exitCode); // 读取退出码
    CloseHandle(processInfo.hProcess); // 关闭进程句柄
    CloseHandle(processInfo.hThread);  // 关闭线程句柄
    return true;
}

LRESULT CALLBACK statusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CTLCOLORSTATIC: {
        // STATIC 默认灰底，改成与主窗口一致的白色
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CLOSE:
        // 更新过程中不允许关闭，避免误关导致误以为程序退出
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

// 构造：初始化安装相关路径
Launcher::Launcher()
{
    installDir_ = getInstallDir();                              // launcher 所在目录
    binDir_ = installDir_ + "\\bin";                            // bin 子目录
    tempDir_ = installDir_ + "\\" + kTempDirName;               // 更新临时目录
    madOnionBoxExe_ = binDir_ + "\\" + kMadOnionBoxExeName;     // 主程序完整路径
    updateListFile_ = tempDir_ + "\\" + kUpdateListFileName;    // 下载的 update.txt
    localVersionFile_ = tempDir_ + "\\" + kLocalVersionFileName; // 本地版本记录
}

Launcher::~Launcher()
{
    closeStatus();
}

// 启动器主流程：建目录 → 安装/更新 → 启动程序
void Launcher::run()
{
    showStatus("正在启动 MadOnionBox...");

    if (!ensureDirectory(binDir_)) { // 确保 bin 目录存在
        closeStatus();
        showError("无法创建 bin 目录: " + binDir_);
        return;
    }
    if (!ensureDirectory(tempDir_)) { // 确保更新临时目录存在
        closeStatus();
        showError("无法创建 temp 目录: " + tempDir_);
        return;
    }

    // 本地还没有主程序：必须安装成功才能继续
    if (!fileExists(madOnionBoxExe_)) {
        if (!installOrUpdateMadOnionBox()) {
            closeStatus();
            showError("MadOnionBox 不存在且自动安装失败。");
            return;
        }
    // 已有主程序：检查更新；失败则提示，但仍尝试启动本地版
    } else if (!installOrUpdateMadOnionBox()) {
        closeStatus();
        showError("检查更新失败，将尝试启动本地版本。");
        // 仍尝试启动本地版，不再提前 return
        showStatus("正在启动本地版本...");
    }

    showStatus("正在启动程序...");
    pumpStatusMessages();
    const bool launched = launchMadOnionBox();
    closeStatus();

    if (!launched) {
        showError("无法启动: " + madOnionBoxExe_);
    }
}

// 根据当前可执行文件路径得到安装根目录
std::string Launcher::getInstallDir() const
{
    char exePath[MAX_PATH] = {0};                   // 存放完整 exe 路径，先清零
    GetModuleFileNameA(nullptr, exePath, MAX_PATH); // nullptr 表示取当前进程模块路径

    std::string dir = exePath;                // 转成 string 方便处理
    const auto pos = dir.find_last_of("\\/"); // 找到最后一个路径分隔符
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos); // 去掉文件名，留下目录
    }
    return dir;
}

// 判断路径是否存在且为普通文件（不是目录）
bool Launcher::fileExists(const std::string &path) const
{
    const DWORD attributes = GetFileAttributesA(path.c_str()); // 查询文件属性
    // 属性有效，并且未设置“目录”标志
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// 递归创建目录；若最终是一个目录则返回 true
bool Launcher::ensureDirectory(const std::string &path) const
{
    if (path.empty()) {
        return false; // 空路径无效
    }

    std::string current;
    current.reserve(path.size()); // 预分配，减少反复扩容

    // 逐字符累积路径，每遇到分隔符就尝试创建到当前这一级
    for (size_t index = 0; index < path.size(); ++index) {
        const char ch = path[index];
        current.push_back(ch);
        if (ch == '\\' || ch == '/') {
            // 跳过盘符根，如 "C:\"（长度通常 <= 3）
            if (current.size() <= 3) {
                continue;
            }
            CreateDirectoryA(current.c_str(), nullptr); // 已存在时失败可忽略
        }
    }

    CreateDirectoryA(path.c_str(), nullptr); // 再创建一次完整目标路径
    const DWORD attributes = GetFileAttributesA(path.c_str());
    // 确认最终路径存在且确实是目录
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// 从本地 version 文件读取 version=；文件不存在或解析失败返回 "0.0.0"
std::string Launcher::readLocalVersion() const
{
    std::ifstream input(localVersionFile_);
    if (!input.is_open()) {
        return "0.0.0";
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    std::string version;
    if (!extractIniValue(buffer.str(), "version", version)) {
        return "0.0.0";
    }
    return version;
}

// 更新成功后：把下载的 update.txt 直接存为本地 version.txt
bool Launcher::saveLocalUpdateFile() const
{
    return CopyFileA(updateListFile_.c_str(), localVersionFile_.c_str(), FALSE) != 0;
}

// 比较两个版本字符串：-1 左小，0 相等，1 左大
int Launcher::compareVersion(const std::string &left, const std::string &right) const
{
    // 把 "1.2.3" / "v1.2" 这类字符串拆成整数数组
    auto parseParts = [](const std::string &version) {
        std::vector<int> parts;
        std::stringstream stream(version);
        std::string segment;
        while (std::getline(stream, segment, '.')) { // 按 '.' 分段
            segment = trim(segment);
            if (segment.empty()) {
                parts.push_back(0); // 空段当 0
                continue;
            }

            // 允许前缀 v/V，如 v1.0.0
            if (!segment.empty() && (segment[0] == 'v' || segment[0] == 'V')) {
                segment.erase(segment.begin());
            }

            try {
                parts.push_back(std::stoi(segment)); // 转成整数
            } catch (...) {
                parts.push_back(0); // 非法片段当 0
            }
        }
        return parts;
    };

    const auto leftParts = parseParts(left);
    const auto rightParts = parseParts(right);
    // 按较长的一方循环，较短方缺省补 0
    const size_t count = std::max(leftParts.size(), rightParts.size());

    for (size_t index = 0; index < count; ++index) {
        const int leftValue = index < leftParts.size() ? leftParts[index] : 0;
        const int rightValue = index < rightParts.size() ? rightParts[index] : 0;
        if (leftValue < rightValue) {
            return -1;
        }
        if (leftValue > rightValue) {
            return 1;
        }
    }
    return 0; // 各段都相等
}

// 下载 update.txt 到 temp，并解析出版本号与全部安装包 URL
bool Launcher::downloadUpdateList(std::string &remoteVersion, std::vector<std::string> &packageUrls) const
{
    if (!ensureDirectory(tempDir_)) {
        return false;
    }

    if (!downloadFile(kUpdateListUrl, updateListFile_)) {
        return false; // 下载失败
    }

    std::ifstream input(updateListFile_);
    if (!input.is_open()) {
        return false; // 打不开刚下载的文件
    }

    std::stringstream buffer;
    buffer << input.rdbuf(); // 把文件全部读入内存
    const std::string text = buffer.str();

    if (!extractIniValue(text, "version", remoteVersion)) {
        return false;
    }
    if (!extractIniValues(text, "packageUrl", packageUrls)) {
        return false;
    }

    return !remoteVersion.empty() && !packageUrls.empty();
}

// 使用 WinHTTP 将 url 下载到 destPath
bool Launcher::downloadFile(const std::string &url, const std::string &destPath) const
{
    // 网络不通时直接失败，避免在 WinHTTP 请求上长时间阻塞
    if (!isNetworkAvailable()) {
        return false;
    }
    // 远程服务器无响应时直接失败，避免进入完整下载流程
    if (!canReachRemoteServer(url)) {
        return false;
    }

    const std::wstring wideUrl = utf8ToWide(url); // WinHTTP 需要宽字符 URL
    URL_COMPONENTS components{};                  // URL 拆解结果容器
    components.dwStructSize = sizeof(components); // 必须设置结构体大小

    wchar_t hostName[256] = {0};  // 主机名缓冲（全零初始化）
    wchar_t urlPath[2048] = {0};  // 路径缓冲（全零初始化）
    components.lpszHostName = hostName; // 告诉 CrackUrl 把主机名写到这里
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostName));
    components.lpszUrlPath = urlPath; // 把路径写到这里
    components.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));

    // 拆解 URL：主机、端口、路径、协议等
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        return false;
    }

    const bool useHttps = components.nScheme == INTERNET_SCHEME_HTTPS; // 是否 HTTPS
    // 打开 WinHTTP 会话（User-Agent、默认代理）
    HINTERNET session = WinHttpOpen(L"MadOnionBoxLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }

    // 连接到目标主机和端口
    HINTERNET connection = WinHttpConnect(session, components.lpszHostName, components.nPort, 0);
    if (!connection) {
        WinHttpCloseHandle(session); // 失败也要释放已打开句柄
        return false;
    }

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0; // HTTPS 需要安全标志
    // 创建 GET 请求
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", components.lpszUrlPath, nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    // 内网自签证书场景：忽略常见证书校验错误（公网慎用）
    if (useHttps) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
    }

    bool success = false;
    // 发送请求并接收响应头
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        // 以二进制、截断方式打开本地目标文件
        std::ofstream output(destPath, std::ios::binary | std::ios::trunc);
        if (output.is_open()) {
            success = true;
            DWORD available = 0;
            do {
                // 查询当前可读取字节数
                if (!WinHttpQueryDataAvailable(request, &available)) {
                    success = false;
                    break;
                }
                if (available == 0) { // 没有更多数据，正常结束
                    break;
                }

                std::vector<char> chunk(available); // 为本块数据分配缓冲
                DWORD downloaded = 0;
                // 读取本块数据
                if (!WinHttpReadData(request, chunk.data(), available, &downloaded)) {
                    success = false;
                    break;
                }
                // 写入本地文件
                output.write(chunk.data(), static_cast<std::streamsize>(downloaded));
                pumpStatusMessages(); // 下载循环中刷新提示窗口，避免显示“未响应”
            } while (available > 0);
        }
    }

    // 无论成败都关闭 WinHTTP 句柄，避免泄漏
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return success;
}

// 解压 zip 到目标目录：先试 tar，失败再试 PowerShell Expand-Archive
bool Launcher::extractZip(const std::string &zipPath, const std::string &destDir) const
{
    ensureDirectory(destDir); // 确保解压目录存在

    // 优先用系统 tar.exe 解压
    const std::string command = "tar.exe -xf \"" + zipPath + "\" -C \"" + destDir + "\"";
    DWORD exitCode = 1;
    if (runHiddenProcess(command, exitCode) && exitCode == 0) {
        return true;
    }

    // 回退方案：PowerShell Expand-Archive
    const std::string psCommand =
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -Path '" + zipPath +
        "' -DestinationPath '" + destDir + "' -Force\"";
    return runHiddenProcess(psCommand, exitCode) && exitCode == 0;
}

// 下载更新列表，必要时依次下载并解压全部安装包
bool Launcher::installOrUpdateMadOnionBox()
{
    showStatus("正在检查更新...");

    std::string remoteVersion;
    std::vector<std::string> packageUrls;
    if (!downloadUpdateList(remoteVersion, packageUrls)) {
        return false; // 拉列表失败
    }

    const std::string localVersion = readLocalVersion();     // 本地版本
    const bool missingBinary = !fileExists(madOnionBoxExe_); // 是否缺少主程序
    // 主程序在，且本地版本 >= 远端版本：无需更新
    if (!missingBinary && compareVersion(localVersion, remoteVersion) >= 0) {
        showStatus("已是最新版本");
        pumpStatusMessages();
        return true;
    }

    for (size_t i = 0; i < packageUrls.size(); ++i) {
        const std::string zipPath = tempDir_ + "\\package_" + std::to_string(i + 1) + ".zip";
        showStatus("正在下载更新包 (" + std::to_string(i + 1) + "/" +
                   std::to_string(packageUrls.size()) + ")...");
        if (!downloadFile(packageUrls[i], zipPath)) {
            return false;
        }
        showStatus("正在解压安装 (" + std::to_string(i + 1) + "/" +
                   std::to_string(packageUrls.size()) + ")...");
        if (!extractZip(zipPath, binDir_)) {
            DeleteFileA(zipPath.c_str());
            return false;
        }
        DeleteFileA(zipPath.c_str());
    }

    if (!fileExists(madOnionBoxExe_)) {
        return false;
    }
    // 成功后把本次 update.txt 存为 temp/version.txt
    if (!saveLocalUpdateFile()) {
        return false;
    }

    showStatus("更新完成");
    pumpStatusMessages();
    return true;
}

// 启动 MadOnionBox.exe
bool Launcher::launchMadOnionBox() const
{
    STARTUPINFOW startupInfo{};
    PROCESS_INFORMATION processInfo{};
    startupInfo.cb = sizeof(startupInfo); // 填结构体大小

    const std::wstring wideExe = ansiToWide(madOnionBoxExe_);
    const std::wstring wideCwd = ansiToWide(installDir_);

    // 以 installDir_ 为工作目录创建进程（宽字符，中文路径更稳妥）
    const BOOL ok = CreateProcessW(wideExe.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                                   wideCwd.c_str(), &startupInfo, &processInfo);
    if (!ok) {
        return false;
    }

    // launcher 不等待子进程结束，关闭句柄即可
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return true;
}

// 弹出错误对话框（宽字符，避免 UTF-8 源码经 ANSI API 显示乱码）
void Launcher::showError(const std::string &message) const
{
    closeStatus(); // 先关掉进度窗，避免挡在错误框后面
    const std::wstring wideMessage = utf8ToWide(message);
    MessageBoxW(nullptr, wideMessage.c_str(), L"MadOnionBox Launcher", MB_OK | MB_ICONERROR);
}

void Launcher::showStatus(const std::string &message) const
{
    constexpr int kWidth = 420;
    constexpr int kHeight = 140;
    const std::wstring wideMessage = utf8ToWide(message);

    if (!statusHwnd_) {
        const wchar_t *kClassName = L"MadOnionBoxLauncherStatus";
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = statusWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_WAIT);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc); // 重复注册失败可忽略

        const int x = (GetSystemMetrics(SM_CXSCREEN) - kWidth) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - kHeight) / 2;
        statusHwnd_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_APPWINDOW, kClassName, L"MadOnionBox",
            WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, kWidth, kHeight, nullptr, nullptr,
            wc.hInstance, nullptr);
        if (!statusHwnd_) {
            return;
        }

        // 禁用关闭按钮，防止更新中途被关掉
        if (HMENU sysMenu = GetSystemMenu(statusHwnd_, FALSE)) {
            EnableMenuItem(sysMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
        }

        statusLabel_ = CreateWindowExW(
            0, L"STATIC", wideMessage.c_str(),
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 20, 30, kWidth - 40, 50,
            statusHwnd_, nullptr, wc.hInstance, nullptr);

        if (!statusFont_) {
            statusFont_ = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        }
        if (statusFont_ && statusLabel_) {
            SendMessageW(statusLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(statusFont_), TRUE);
        }

        ShowWindow(statusHwnd_, SW_SHOW);
        UpdateWindow(statusHwnd_);
    } else if (statusLabel_) {
        SetWindowTextW(statusLabel_, wideMessage.c_str());
    }

    pumpStatusMessages();
}

void Launcher::closeStatus() const
{
    if (statusHwnd_) {
        DestroyWindow(statusHwnd_);
        statusHwnd_ = nullptr;
        statusLabel_ = nullptr;
    }
    if (statusFont_) {
        DeleteObject(statusFont_);
        statusFont_ = nullptr;
    }
}

void Launcher::pumpStatusMessages() const
{
    MSG msg{};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// Windows GUI 子系统入口（无控制台）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Launcher launcher; // 构造启动器
    launcher.run();    // 执行主流程
    return 0;
}
