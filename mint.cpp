#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/scope_exit.hpp>
#include <windows.h>

namespace {

std::string const AppID = "iorate.mint.0";
std::string const AppName = "mint MSYS2 Launcher";

constexpr int OffsetX = 24;
constexpr int OffsetY = 24;

BOOL bRet;
DWORD dwRet;

class MintError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

[[noreturn]] void ThrowLastError(std::string const &msg)
{
    char *buf;
    dwRet = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        GetLastError(),
        LANG_USER_DEFAULT,
        reinterpret_cast<LPTSTR>(&buf),
        0,
        nullptr);
    if (dwRet == 0) throw MintError("FormatMessage() failed");
    BOOST_SCOPE_EXIT_ALL(&) { LocalFree(buf); };
    throw MintError(msg + ": " + buf);
}

std::string ReplaceFilename(std::string const &path, std::string const &filename)
{
    auto newPath = path;
    auto const sep = newPath.rfind('\\');
    if (sep != std::string::npos) newPath.replace(sep + 1, std::string::npos, filename);
    return newPath;
}

} // anonymous namespace

int main(int argc, char **argv)
try
{
    using namespace std::string_literals;

    namespace po = boost::program_options;
    namespace pt = boost::property_tree;

    po::options_description optDescr;
    optDescr.add_options()
        ("ini", po::value<std::string>(), "Spefify ini file")
        ("runas", "Run as administrator");
    po::variables_map varMap;
    try {
        po::store(
            po::parse_command_line(
                argc, argv, optDescr,
                po::command_line_style::default_style |
                po::command_line_style::allow_slash_for_short |
                po::command_line_style::allow_long_disguise),
            varMap);
    } catch (po::error const &err) {
        throw MintError("Invalid option: "s + err.what());
    }
    po::notify(varMap);

    std::string exePath(MAX_PATH, '\0');
    dwRet = GetModuleFileName(nullptr, &exePath[0], exePath.size());
    if (dwRet == 0) ThrowLastError("GetModuleFileName() failed");
    exePath.resize(dwRet);

    if (varMap.count("runas")) {
        // Elevated?
        HANDLE token;
        bRet = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        if (bRet == FALSE) ThrowLastError("GetCurrentProcess() failed");
        BOOST_SCOPE_EXIT_ALL(&) { CloseHandle(token); };
        TOKEN_ELEVATION elevation;
        DWORD len;
        bRet = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &len);
        if (bRet == FALSE) ThrowLastError("GetTokenInformation() failed");

        if (elevation.TokenIsElevated == 0) {
            // Re-run as administrator
            SHELLEXECUTEINFO execInfo = { sizeof(SHELLEXECUTEINFO) };
            execInfo.lpVerb = "runas";
            execInfo.lpFile = exePath.c_str();
            std::string cmdLine;
            if (varMap.count("ini")) cmdLine = "/ini " + varMap["ini"].as<std::string>();
            execInfo.lpParameters = cmdLine.c_str();
            bRet = ShellExecuteEx(&execInfo);
            if (bRet == FALSE) ThrowLastError("Failed to run as administrator");
            return 0;
        }
    }

    // Load ini file
    auto const iniPath = varMap.count("ini") ?
        varMap["ini"].as<std::string>() :
        ReplaceFilename(exePath, "mint.ini");

    pt::ptree ini;
    std::ifstream iniStr(iniPath);
    if (iniStr) {
        try {
            pt::read_ini(iniStr, ini);
        } catch (pt::ini_parser_error const &err) {
            throw MintError("Failed to parse ini: "s + err.what());
        }
        iniStr.close();
    }

    // Options
    auto const iconPath = ini.get_optional<std::string>("mint.icon").value_or_eval([&]
        {
            return ReplaceFilename(exePath, "msys2.ico");
        });
    auto const minttyPath = ini.get_optional<std::string>("mint.mintty").value_or_eval([&]
        {
            return ReplaceFilename(exePath, "usr\\bin\\mintty.exe");
        });
    auto const minttyPos = ini.get_optional<std::string>("mint.minttyPos").value_or("mintty");

    // Set environment variables
    if (auto const envNode = ini.get_child_optional("env")) {
        for (auto const &node : *envNode) {
            bRet = SetEnvironmentVariable(node.first.c_str(), node.second.data().c_str());
            if (bRet == FALSE) ThrowLastError("Invalid environment variable");
        }
    }

    // Launch mintty
    std::ostringstream cmdLineStr;
    auto const prevMintty = FindWindow("mintty", nullptr);
    if (prevMintty == nullptr) {
        cmdLineStr << minttyPos;
    } else {
        RECT rect;
        bRet = GetWindowRect(prevMintty, &rect);
        if (bRet == FALSE) ThrowLastError("GetWindowRect() failed");
        cmdLineStr << "mintty -o X=" << (rect.left + OffsetX) << " -o Y=" << (rect.top + OffsetY);
    }
    cmdLineStr
        << " -i \"" << iconPath << "\""
        << " -o Class=mintty"
        << " -o AppID=\"" << AppID << "\""
        << " -o AppName=\"" << AppName << "\""
        << " -o AppLaunchCmd=\"" << exePath << "\""
        << " -R o"
        << " --store-taskbar-properties"
        << " -";
    auto cmdLine = cmdLineStr.str();

    HANDLE readPipe, writePipe;
    SECURITY_ATTRIBUTES pipeSA = { sizeof(SECURITY_ATTRIBUTES) };
    pipeSA.bInheritHandle = TRUE;
    bRet = CreatePipe(&readPipe, &writePipe, &pipeSA, 0);
    if (bRet == FALSE) ThrowLastError("CreatePipe() failed");
    BOOST_SCOPE_EXIT_ALL(&) {
        CloseHandle(readPipe);
        if (writePipe != nullptr) CloseHandle(writePipe);
    };

    STARTUPINFO startupInfo = { sizeof(STARTUPINFO) };
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = writePipe;

    PROCESS_INFORMATION procInfo = {};

    bRet = CreateProcess(
        minttyPath.c_str(),
        &cmdLine[0],
        nullptr,
        nullptr,
        TRUE,
        NORMAL_PRIORITY_CLASS,
        nullptr,
        nullptr, // mintty will have the same current directory as mint
        &startupInfo,
        &procInfo);
    if (bRet == FALSE) ThrowLastError("Failed to start mintty");
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);
    CloseHandle(std::exchange(writePipe, nullptr));

    std::string posReport(301, '\0');
    DWORD posReportSize;
    bRet = ReadFile(readPipe, &posReport[0], posReport.size(), &posReportSize, nullptr);
    if (bRet == FALSE) ThrowLastError("ReadFile() failed");
    posReport.resize(posReportSize);

    ini.put("mint.minttyPos", posReport);

    try {
        pt::write_ini(iniPath, ini);
    } catch (pt::ini_parser_error const &err) {
        throw MintError("Failed to write ini file: "s + err.what());
    }
}
catch (MintError const &e)
{
    MessageBox(nullptr, e.what(), nullptr, MB_OK);
}
