# mint
Yet another MSYS2 launcher. It can
* suppress the blinking of Command Prompt window at startup
* enable taskbar pinning
* save and restore the position and size of mintty window
* run as administrator by command line option

## Install
`make` and move `mint.exe` to the MSYS2 root. A recent version of GCC, and Boost C++ Libraries requried.

## Command Line Options
    /mint [OPTION]... [COMMAND]
    Options:
    -c [ --config ] PATH  specify the ini file to load and save
    -r [ --runas ]        run as administrator

If COMMAND is specified, it is executed by `sh -lc` with the current directory inherited. You can use `/mint -r <COMMAND>` a bit like `sudo <COMMAND>`.

## Configuration
If you rename `mint.exe` to `<mint-name>.exe`, it will load `<mint-name>.ini` in the same folder by default. Therefore, you can rename `mint.exe` to `msys.exe`, `mingw32.exe` and `mingw64.exe` in the MSYS2 folder and use them separately, just like [MSYS2 Launcher](https://github.com/elieux/msys2-launcher).

In the .ini file, you can configure environment variables used in MSYS2 startup, `mintty.exe` path and icon path.

Example:

    [Env]
    # Set environment variables
    # In particular, MSYSTEM, MSYS, MSYS2_PATH_TYPE, CHERE_INVOKING
    # affect the behavior of MSYS2
    MSYSTEM=MINGW64
    MSYS=winsymlinks:nativestrict
    # MSYS2_PATH_TYPE=inherit
    CHERE_INVOKING=1

    [Config]
    # Set mintty.exe path (default: <mint-dir>\usr\bin\mintty.exe)
    # Mintty=C:\msys64\usr\bin\mintty.exe
    # Set icon path (default: <mint-dir>\msys2.ico>)
    # Icon=C:\msys64\msys2.ico
