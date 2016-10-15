#include <exception>
#include <iterator>
#include <regex>
#include <string>
#include <utility>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/scope_exit.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include "getoptmm/getoptmm.hpp"
#include <windows.h>

using namespace std::string_literals;

namespace {

struct Exit
{
    Exit() = default;
    explicit Exit(std::string const &msg) : Message(msg) {}
    boost::optional<std::string> Message;
};

struct Options
{
    boost::optional<std::wstring> Config;
    bool Runas = false;
    std::vector<std::wstring> Command;
};

std::wstring BuildCommandLine(std::vector<std::wstring> const &args)
{
    std::wstring cmdLine;
    bool first = true;
    for (auto const &arg : args) {
        if (!std::exchange(first, false)) cmdLine += L' ';
        cmdLine += L'"' + std::regex_replace(arg, std::wregex(LR"#((\\*)")#"), LR"($1$1\")") + L'"';
    }
    return cmdLine;
}

boost::system::system_error LastError()
{
    return { static_cast<int>(GetLastError()), boost::system::system_category() };
}

}

int wmain(int argc, wchar_t **argv)
try {
    namespace fs = boost::filesystem;
    namespace pt = boost::property_tree;

    BOOL bRet;
    DWORD dwRet;

    auto const args = std::vector<std::wstring>(argv + 1, argv + argc);

    auto const options = [&] {
        using namespace getoptmm;
        Options options;
        woption const desc[] = {
            { { L'c' }, { L"config" }, required_arg, assign(options.Config), L"", L"" },
            { { L'r' }, { L"runas" }, no_arg, assign_true(options.Runas), L"" } };
        wparser p(
            std::begin(desc), std::end(desc), push_back(options.Command),
            parse_flag::posixly_correct);
        try {
            p.run(argc, argv);
        } catch (wparser::error const &e) {
            MessageBox(nullptr, (L"Failed to parse: " + e.message()).c_str(), nullptr, MB_OK);
            throw Exit();
        }
        return options;
    }();

    auto const exePath = [&] {
        wchar_t filename[MAX_PATH];
        bRet = GetModuleFileName(nullptr, filename, MAX_PATH);
        if (bRet == FALSE) throw LastError();
        return fs::path(filename);
    }();

    if (options.Runas) {
        auto const isElevated = [&] {
            HANDLE token;
            bRet = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
            if (bRet == FALSE) throw LastError();
            BOOST_SCOPE_EXIT_ALL(&) { CloseHandle(token); };
            TOKEN_ELEVATION elev;
            DWORD elevLen;
            bRet = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &elevLen);
            if (bRet == FALSE) throw LastError();
            return elev.TokenIsElevated != 0;
        }();

        if (!isElevated) {
            SHELLEXECUTEINFO execInfo = { sizeof(execInfo) };
            execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
            execInfo.lpVerb = L"runas";
            execInfo.lpFile = exePath.c_str();
            auto const cmdLine = BuildCommandLine(args);
            execInfo.lpParameters = cmdLine.c_str();
            bRet = ShellExecuteEx(&execInfo);
            if (bRet == FALSE) throw Exit();
            BOOST_SCOPE_EXIT_ALL(&) { CloseHandle(execInfo.hProcess); };

            dwRet = WaitForSingleObject(execInfo.hProcess, INFINITE);
            if (dwRet == WAIT_FAILED) throw LastError();
            return 0;
        }
    }

    auto const iniPath = [&] {
        if (options.Config) {
            auto const p = fs::path(*options.Config);
            return p.is_absolute() ? p : exePath.parent_path() / p;
        }
        return fs::path(exePath).replace_extension(L"ini");
    }();

    auto const config = [&] {
        if (auto &&ifs = fs::wifstream(iniPath)) {
            pt::wptree tree;
            try {
                pt::read_ini(ifs, tree);
            } catch (pt::ini_parser_error const &e) {
                throw Exit("Failed to read ini: "s + e.what());
            }
            return tree;
        }
        return pt::wptree();
    }();

    auto const setEnv = [&](std::wstring const &name, std::wstring const &value) {
        bRet = SetEnvironmentVariable(name.c_str(), value.c_str());
        if (bRet == FALSE) {
            throw Exit("Failed to set environment variable: "s + LastError().what());
        }
    };
    if (auto const env = config.get_child_optional(L"Env")) {
        for (auto const &nvp : *env) {
            setEnv(nvp.first, nvp.second.data());
        }
    }
    setEnv(L"MSYSCON", L"mintty.exe");
    if (!options.Command.empty()) setEnv(L"CHERE_INVOKING", L"1");

    auto const minttyPos = [&]
    {
        auto const loadPath = [&](auto const &key, auto const &default_) {
            auto const p = config.get_optional<fs::path>(key).value_or(default_);
            return p.is_absolute() ? p : exePath.parent_path() / p;
        };
        auto const minttyPath = loadPath(L"Config.Mintty", L"usr\\bin\\mintty.exe");

        auto const program = options.Command.empty() ?
                L"-" :
                L"/usr/bin/sh -lc '\"$@\"' sh " + BuildCommandLine(options.Command);
        auto cmdLine =
            config.get_optional<std::wstring>(L"Config.MinttyPos").value_or(L"mintty") +
            L" -i \"" + loadPath(L"Config.Icon", L"msys2.ico").native() + L"\"" +
            L" -o AppID=\"iorate.mint." + exePath.stem().native() + L"\"" +
            L" -o AppName=\"" + exePath.stem().native() + L"\"" +
            L" -o AppLaunchCmd=\"" + exePath.native() + L"\"" +
            L" -R o" +
            L" --store-taskbar-properties" +
            L" " + program;

        HANDLE readPipe, writePipe;
        SECURITY_ATTRIBUTES pipeAttr = { sizeof(pipeAttr), nullptr, TRUE };
        bRet = CreatePipe(&readPipe, &writePipe, &pipeAttr, 0);
        if (bRet == FALSE) throw LastError();
        BOOST_SCOPE_EXIT_ALL(&) {
            CloseHandle(readPipe);
            if (writePipe != nullptr) CloseHandle(writePipe);
        };
        STARTUPINFO startupInfo = { sizeof(startupInfo) };
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = writePipe;

        PROCESS_INFORMATION procInfo;

        bRet = CreateProcess(
            minttyPath.c_str(), &cmdLine[0], nullptr, nullptr, TRUE, NORMAL_PRIORITY_CLASS,
            nullptr, nullptr, &startupInfo, &procInfo);
        if (bRet == FALSE) throw Exit("Failed to create process: "s + LastError().what());
        CloseHandle(procInfo.hThread);
        CloseHandle(procInfo.hProcess);

        CloseHandle(std::exchange(writePipe, nullptr));
        std::string posReport(256, '\0');
        DWORD posReportSize;
        bRet = ReadFile(readPipe, &posReport[0], posReport.size(), &posReportSize, nullptr);
        if (bRet == FALSE) throw Exit();
        posReport.resize(posReportSize);
        return posReport;
    }();

    pt::wptree configAfter(config);
    configAfter.put(L"Config.MinttyPos", std::wstring(minttyPos.begin(), minttyPos.end()));
    if (auto &&ofs = fs::wofstream(iniPath)) {
        try {
            pt::write_ini(ofs, configAfter);
        } catch (pt::ini_parser_error const &e) {
            throw Exit("Failed to write ini: "s + e.what());
        }
    } else {
        throw Exit("Failed to write ini");
    }
} catch (Exit const &e) {
    if (e.Message) MessageBoxA(nullptr, e.Message->c_str(), nullptr, MB_OK);
} catch (std::exception const &e) {
    MessageBoxA(nullptr, ("Unexpected error occurred: "s + e.what()).c_str(), nullptr, MB_OK);
} catch (...) {
}
