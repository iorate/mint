# mint
Yet another MSYS2 launcher. It can
* suppress the blinking of Command Prompt window at startup
* enable taskbar pinning
* save and restore the position and size of mintty window
* run as administrator by command line option

## Install
`make` and move `mint.exe` to the MSYS2 root. A recent version of GCC, and Boost C++ Libraries requried.

In the following, `<mint-dir>` stands for the directory containing `mint.exe`.
## Command Line Options
    /ini PATH   specify the ini file to load and save
                default: <mint-dir>\mint.ini
    /runas      run as administrator
## Configuration
Edit mint.ini, or the ini file specified by /ini option.

Example:

    [env]
    # Set environment variables
    # In particular, MSYSTEM, MSYS, MSYS2_PATH_TYPE, CHERE_INVOKING
    # affects the behavior of MSYS2
    MSYSTEM=MINGW64
    CHERE_INVOKING=1

    [mint]
    # Set mintty.exe path (default: <mint-dir>\usr\bin\mintty.exe)
    mintty=C:\Installed\MSYS\usr\bin\mintty.exe
    # Set icon path (default: <mint-dir>\msys2.ico>)
    icon=C:\Installed\MSYS\msys2.ico
    # The command line specifying position and size of window
    # It is read and written by mint.exe
    minttyPos=mintty -o Columns=80 -o Rows=24 -o X=1859 -o Y=814
