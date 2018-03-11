
// mint
//
// Copyright iorate 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define MINT_VERSION "2"

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
// #include <iostream>
#include <locale>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include <stdio.h> // for _wfopen and fgetws
#include <windows.h>
#include <nonsugar.hpp>

#ifdef _MSC_VER
#if _MSC_VER <= 1913
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif
#else
namespace fs = std::filesystem;
#endif

namespace {

struct scope_exit
{
    template <class F>
    struct impl
    {
        impl(impl const &) = delete;
        impl &operator=(impl const &) = delete;

        ~impl() { m_f(); }

        F m_f;
    };

    template <class F>
    impl<F> operator^(F &&f) const
    {
        return {std::move(f)};
    }
};

#define PP_CAT_I(x, y) x ## y
#define PP_CAT(x, y) PP_CAT_I(x, y)
#define SCOPE_EXIT \
    auto const &PP_CAT(scope_exit_, __LINE__) [[maybe_unused]] = ::scope_exit() ^ [&] \
    /**/

class error : public std::exception
{
public:
    explicit error(std::wstring const &message) :
        std::exception(),
        m_message(message)
    {}

    std::wstring message() const
    {
        return m_message;
    }

private:
    std::wstring m_message;
};

class panic : public std::exception
{
};

using environment_type = std::vector<std::pair<std::wstring, std::wstring>>;

struct configuration
{
    fs::path mintty_path;
    fs::path icon_path;
    fs::path winpos_path;
    environment_type environment;
};

enum class message_box_icon : UINT
{
    information = MB_ICONINFORMATION,
    warning = MB_ICONWARNING,
    error = MB_ICONERROR
};

void message_box(std::wstring const &message, message_box_icon icon)
{
    MessageBox(nullptr, message.c_str(), L"m2", static_cast<UINT>(icon));
}

bool is_running_as_administrator()
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        throw panic();
    }
    TOKEN_ELEVATION elev;
    DWORD len;
    if (!GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &len)) {
        throw panic();
    }
    return elev.TokenIsElevated;
}

fs::path get_executable_path()
{
    constexpr DWORD buf_size = 32768;
    std::wstring buf(buf_size, L'\0');
    auto const n = GetModuleFileName(nullptr, buf.data(), buf_size);
    if (!n) {
        throw panic();
    }
    buf.resize(n);
    return buf;
}

std::wstring generate_command_line(std::vector<std::wstring> const &args)
{
    std::wstring cmd_line;
    bool fst = true;
    for (auto const &arg : args) {
        if (!std::exchange(fst, false)) {
            cmd_line += L' ';
        }
        cmd_line += L'"';
        // Escape double quotation mark and preceding backslashes (e.g. \\" -> \\\\\")
        cmd_line += std::regex_replace(arg, std::wregex(LR"#((\\*)")#"), LR"($1$1\")");
        cmd_line += L'"';
    }
    return cmd_line;
}

int run_as_administrator(std::vector<std::wstring> const &args)
{
    auto const exe_path = get_executable_path();
    auto const cmd_line = generate_command_line(args);

    SHELLEXECUTEINFO exec_info = {};
    exec_info.cbSize = sizeof(exec_info);
    exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec_info.lpVerb = L"runas";
    exec_info.lpFile = exe_path.c_str();
    exec_info.lpParameters = cmd_line.c_str();
    if (!ShellExecuteEx(&exec_info)) {
        if (GetLastError() != ERROR_CANCELLED) {
            throw panic();
        }
        // The user cancelled the promotion
        return 1;
    }
    SCOPE_EXIT {
        CloseHandle(exec_info.hProcess);
    };

    if (WaitForSingleObject(exec_info.hProcess, INFINITE) != WAIT_OBJECT_0) {
        throw panic();
    }
    DWORD exit_code;
    if (!GetExitCodeProcess(exec_info.hProcess, &exit_code)) {
        throw panic();
    }
    return static_cast<int>(exit_code);
}

std::optional<std::wstring> get_environment_variable(std::wstring const &name)
{
    constexpr DWORD buf_size = 2048;
    std::wstring buf(buf_size, L'\0');
    auto const n = GetEnvironmentVariable(name.c_str(), buf.data(), buf_size);
    if (!n) {
        return std::nullopt;
    }
    buf.resize(n);
    return buf;
}

fs::path get_home_directory()
{
    if (auto const home = get_environment_variable(L"HOME")) {
        return *home;
    }
    if (auto const home_drive = get_environment_variable(L"HOMEDRIVE")) {
        auto const home_path = get_environment_variable(L"HOMEPATH").value_or(L"");
        return *home_drive + home_path;
    }
    if (auto const user_profile = get_environment_variable(L"USERPROFILE")) {
        return *user_profile;
    }
    return L"C:";
}

std::wstring trim(std::wstring const &s)
{
    return std::regex_replace(s, std::wregex(LR"(^\s+|\s+$)"), L"");
}

template <class Map>
typename Map::mapped_type const *get_pointer(Map const &m, typename Map::key_type const &k)
{
    auto const it = m.find(k);
    if (it == m.end()) {
        return nullptr;
    }
    return &it->second;
}

configuration read_rc(fs::path const &rc_path)
{
    struct iless
    {
        bool operator()(std::wstring const &s1, std::wstring const &s2) const
        {
            return std::lexicographical_compare(
                s1.begin(), s1.end(), s2.begin(), s2.end(),
                [](wchar_t c1, wchar_t c2)
                {
                    auto const &loc = std::locale::classic();
                    return std::tolower(c1, loc) < std::tolower(c2, loc);
                }
                );
        }
    };
    using section_type = std::map<std::wstring, std::wstring, iless>;

    auto const stream = _wfopen(rc_path.c_str(), L"rt, ccs=UTF-8");
    if (!stream) {
        throw error(L"cannot open rc file: " + rc_path.native());
    }
    SCOPE_EXIT {
        fclose(stream);
    };

    constexpr int buf_size = 80;
    wchar_t buf[buf_size];
    std::wstring line;

    std::wregex const comment_regex(LR"(\s*([;#].*)?)");
    std::wregex const section_regex(LR"(\s*\[([^\]]*)\].*)");
    std::wregex const key_value_regex(LR"(([^=]*)=(.*))");

    std::map<std::wstring, section_type, iless> sections;
    std::optional<std::wstring> cur_section_name;
    section_type cur_section;

    while (fgetws(buf, buf_size, stream)) {
        std::wstring s = buf;
        // s may be empty when the first character is L'\0'
        if (s.empty() || s.back() != L'\n') {
            line += s;
            continue;
        }
        s.pop_back();
        line += s;

        std::match_results<std::wstring::const_iterator> m;
        if (std::regex_match(line, comment_regex)) {
            // (empty)
            // ;comment
            // #comment
        } else if (std::regex_match(line, m, section_regex)) {
            // [section]
            if (cur_section_name) {
                sections.insert({*cur_section_name, std::move(cur_section)});
            }
            cur_section_name = trim(m.str(1));
            cur_section.clear();
        } else if (std::regex_match(line, m, key_value_regex)) {
            // key=value
            cur_section.insert({trim(m.str(1)), trim(m.str(2))});
        } else {
            throw error(L"cannot parse rc file: " + rc_path.native() + L": " + line);
        }

        line.clear();
    }
    if (cur_section_name) {
        sections.insert({*cur_section_name, std::move(cur_section)});
    }

    // Dump rc file
    /*
    std::wcout << L"{\n";
    for (auto const &[n, s] : sections) {
        std::wcout << L"  \"" << n << L"\": {\n";
        for (auto const &[k, v] : s) {
            std::wcout << L"    \"" << k << L"\": \"" << v << "\",\n";
        }
        std::wcout << L"  },\n";
    }
    std::wcout << L"}\n";
    */

    configuration conf;
    fs::path const msys2_root = get_environment_variable(L"MSYS2_ROOT").value_or(L"C:\\msys64");
    conf.mintty_path = msys2_root / L"usr\\bin\\mintty.exe";
    conf.icon_path = msys2_root / L"msys2.ico";
    conf.winpos_path = get_home_directory() / L".m2winpos";
    if (auto const path = get_pointer(sections, L"path")) {
        if (auto const mintty = get_pointer(*path, L"mintty")) {
            conf.mintty_path = *mintty;
        }
        if (auto const icon = get_pointer(*path, L"icon")) {
            conf.icon_path = *icon;
        }
        if (auto const winpos = get_pointer(*path, L"winpos")) {
            conf.winpos_path = *winpos;
        }
    }
    if (auto const environment = get_pointer(sections, L"environment")) {
        conf.environment.assign(environment->begin(), environment->end());
    }

    return conf;
}

configuration read_ini(fs::path const &ini_path)
{
    configuration conf;
    auto const msys2_root = get_executable_path().parent_path();
    conf.mintty_path = msys2_root / L"usr\\bin\\mintty.exe";
    conf.icon_path = msys2_root / L"msys2.ico";
    conf.winpos_path = get_home_directory() / L".m2winpos";

    std::wifstream stream(ini_path);
    if (!stream) {
        return conf;
    }
    std::wstring line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.front() == L'#') {
            continue;
        }
        auto const n = line.find(L'=');
        if (n == std::wstring::npos) {
            throw error(L"cannot parse ini file: " + ini_path.native() + L": " + line);
        }
        conf.environment.push_back({line.substr(0, n), line.substr(n + 1)});
    }

    return conf;
}

void set_environment_variable(std::wstring const &name, std::wstring const &value)
{
    if (!SetEnvironmentVariable(name.c_str(), value.c_str())) {
        throw error(L"cannot set environment variable: " + name);
    }
}

std::wstring expand_environment_variables(std::wstring const &s)
{
    DWORD buf_size = 2048;
    std::wstring buf(buf_size, L'\0');
    auto n = ExpandEnvironmentStrings(s.c_str(), buf.data(), buf_size);
    if (!n) {
        throw panic();
    }
    if (n > buf_size) {
        buf_size = n;
        buf.resize(buf_size);
        n = ExpandEnvironmentStrings(s.c_str(), buf.data(), buf_size);
        if (!n) {
            throw panic();
        }
    }
    buf.resize(n - 1);
    return buf;
}

std::wstring launch_mintty(
    fs::path const &mintty_path,
    fs::path const &icon_path,
    std::optional<std::wstring> const &winpos,
    std::vector<std::wstring> const &command
    )
{
    // Build command line
    auto const msystem = get_environment_variable(L"MSYSTEM");
    if (!msystem) {
        throw panic();
    }
    auto cmd_line = winpos.value_or(L"mintty");
    cmd_line += L" ";
    cmd_line += generate_command_line({
        L"-i", icon_path.native(),
        L"-o", L"AppID=iorate.mint." MINT_VERSION,
        L"-o", L"AppName=" + get_executable_path().stem().native(),
        L"-o", L"AppLaunchCmd=" + get_executable_path().native(),
        L"-t", L"MSYS2 " + *msystem + L" Shell",
        L"-R", L"s",
        L"--store-taskbar-properties"
        });
    if (command.empty()) {
        cmd_line += L" -";
    } else {
        cmd_line += L" /usr/bin/sh -lc '\"$@\"' sh ";
        cmd_line += generate_command_line(command);
    }

    // Create pipes
    HANDLE read_pipe;
    HANDLE write_pipe;
    SECURITY_ATTRIBUTES pipe_attrib = { sizeof(pipe_attrib), nullptr, TRUE };
    if (!CreatePipe(&read_pipe, &write_pipe, &pipe_attrib, 0)) {
        throw panic();
    }
    SCOPE_EXIT {
        if (write_pipe) {
            CloseHandle(write_pipe);
        }
        CloseHandle(read_pipe);
    };

    STARTUPINFO startup_info = {};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdOutput = write_pipe;

    PROCESS_INFORMATION proc_info;

    if (!CreateProcess(
            mintty_path.c_str(),
            cmd_line.data(),
            nullptr,
            nullptr,
            TRUE, // inherit handles
            NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &startup_info,
            &proc_info
            )) {
        throw error(L"cannot launch mintty: " + mintty_path.native());
    }
    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);

    CloseHandle(std::exchange(write_pipe, nullptr));
    constexpr DWORD buf_size = 80;
    std::string buf(buf_size, '\0');
    DWORD n;
    if (!ReadFile(read_pipe, buf.data(), buf_size, &n, nullptr)) {
        throw error(L"something went wrong with mintty");
    }
    buf.resize(n);
    return std::wstring(buf.begin(), buf.end());
}

} // unnamed namespace

int wmain(int argc, wchar_t **argv)
try {
    // Parse command line
    auto const cmd = nonsugar::wcommand<char>(L"m2")
        .flag<'h'>({L'h'}, {L"help"}, L"show help (this message) and exit")
        .flag<'v'>({L'v'}, {L"version"}, L"show version information and exit")
        .flag<'r'>({L'r'}, {L"runas"}, L"run as administrator")
        .flag<'i', std::wstring>({L'i'}, {L"init"}, L"<m2rc>", L"use <m2rc> instead of ~/.m2rc")
        .argument<'c', std::vector<std::wstring>>(L"COMMAND")
        ;
    auto const opts = nonsugar::parse(argc, argv, cmd);
    if (opts.has<'h'>()) {
        message_box(nonsugar::usage(cmd), message_box_icon::information);
        return 0;
    }
    if (opts.has<'v'>()) {
        message_box(L"mint version 2.1", message_box_icon::information);
        return 0;
    }

    // Run as administrator if required
    if (opts.has<'r'>() && !is_running_as_administrator()) {
        return run_as_administrator({argv + 1, argv + argc});
    }

    // Load configuration
    std::optional<fs::path> rc_path = opts.get_optional<'i'>();
    if (!rc_path) {
        auto const def_rc_path = get_home_directory() / L".m2rc";
        if (fs::exists(def_rc_path)) {
            rc_path = def_rc_path;
        }
    }
    configuration conf;
    if (rc_path) {
        conf = read_rc(*rc_path);
    } else {
        auto const ini_path = get_executable_path().replace_extension(L".ini");
        conf = read_ini(ini_path);
    }
    // Dump configuration
    /*
    std::wcout << L"{\n";
    std::wcout << L"  \"mintty_path\": \"" << conf.mintty_path << L"\",\n";
    std::wcout << L"  \"icon_path\": \"" << conf.icon_path << L"\",\n";
    std::wcout << L"  \"winpos_path\": \"" << conf.winpos_path << L"\",\n";
    std::wcout << L"  \"environment\": {\n";
    for (auto const &[k, v] : conf.environment) {
        std::wcout << L"    \"" << k << L"\": \"" << v << L"\",\n";
    }
    std::wcout << L"  },\n";
    std::wcout << L"}\n";
    */

    // Set environment variables
    set_environment_variable(L"MSYSTEM", L"MSYS");
    if (!opts.get<'c'>().empty()) {
        set_environment_variable(L"CHERE_INVOKING", L"1");
    }
    for (auto const &[k, v] : conf.environment) {
        set_environment_variable(k, expand_environment_variables(v));
    }
    set_environment_variable(L"MSYSCON", L"mintty.exe");

    // Launch mintty
    std::optional<std::wstring> winpos;
    if (std::wifstream stream(conf.winpos_path); stream) {
        if (std::wstring line; std::getline(stream, line)) {
            winpos = line;
        }
    }
    auto const new_winpos = launch_mintty(
        conf.mintty_path,
        conf.icon_path,
        winpos,
        opts.get<'c'>()
        );
    if (std::wofstream stream(conf.winpos_path); !stream) {
        throw error(L"cannot write winpos file: " + conf.winpos_path.native());
    } else {
        stream << new_winpos;
    }

    return 0;
} catch (nonsugar::werror const &e) {
    message_box(e.message().substr(4), message_box_icon::warning); // substr(4) drops "m2: "
    return 1;
} catch (error const &e) {
    message_box(e.message(), message_box_icon::warning);
    return 1;
} catch (std::exception const &) {
    message_box(L"unexpected error occurred", message_box_icon::error);
    return 1;
}
