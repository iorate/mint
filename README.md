# mint
Yet another MSYS2 launcher. It can
* suppress the blinking of Command Prompt window at startup
* enable taskbar pinning
* save and restore the position and size of mintty window
* run as administrator by command line option

## Install
Build and move `m2.exe` to the MSYS2 root. Visual C++ 14.1 and CMake is required.

## Command Line Options
```
Usage: m2 [OPTION...] [COMMAND...]

Options:
  -h         --help         show help (this message) and exit
  -v         --version      show version information and exit
  -r         --runas        run as administrator
  -i <m2rc>  --init=<m2rc>  use <m2rc> instead of ~/.m2rc
```
If `COMMAND` is specified, it is executed by `sh -lc` in the current directory. You can use `/m2 -r <COMMAND>` a bit like `sudo <COMMAND>`.

## Configuration
By default, `m2.exe` loads `m2.ini` in the same foler if it exists. If you rename `m2.exe` to `<APPNAME>.exe`, it will load `<APPNAME>.ini`. Therefore, you can rename `m2.exe` to `msys2.exe`, `mingw32.exe` and `mingw64.exe` and use them separately, just like [MSYS2 Launcher](https://github.com/elieux/msys2-launcher).

In ini file, you can configure environment variables used in MSYS2 startup.

Example:
```INI
# Set environment variables.
# It is useful to set enviroment variables that affect MSYS2 startup,
# such as MSYSTEM and CHERE_INVOKING.
MSYSTEM=MINGW64
CHERE_INVOKING=1
MSYS=winsymlinks:nativestrict
# MSYS2_PATH_TYPE=inherit
# MSYS2_ARG_CONV_EXCL=
```
You can locate `m2.exe` in any folder and use m2rc file instead of ini file. In m2rc file, you can configure `mintty.exe` path and the icon path as well as environment variables. By default, `~/.m2rc` is loaded if it exists.

Example:
```INI
[Path]
mintty = C:\Apps\MSYS2\usr\bin\mintty.exe
icon = C:\Apps\MSYS2\msys2.ico
; winpos specifies the file to which the window position of mintty is saved.
; winpos = C:\Users\iorate\.m2winpos

[Environment]
MSYSTEM = MINGW64
CHERE_INVOKING = 1
MSYS2_PATH_TYPE = strict
```
## Author
[@iorate](https://twitter.com/iorate)

## License
[Boost Software License 1.0](http://www.boost.org/LICENSE_1_0.txt)
