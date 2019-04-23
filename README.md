LidLock
======
Lock Windows laptop when lid is closed


## Introduction
LidLock is a simple app written in C for Windows to automatically lock the screen when the lid is closed. This resembles the same behavior as macOS, when one needs the laptop not to sleep but just to be locked when closing the lid.

The app also works with clamshell mode and external monitors. It will ignore locking if lid is open and external monitors are connected, and re-locks if the lid is closed and external monitors are disconnected.

## Usage
LidLock is a single portable executable file. Simply double-click the executable and it silently runs as a daemon in the background without any windows, prompts or icons. It listens to relevant events and does not consume CPU when waiting.

If you want to stop LidLock, you can use Task Manager to stop the process, or run the following command in cmd:
```cmd
taskkill /f /im lidlock.exe /t
```

To make it run at startup, create a shortcut of `lidlock.exe` and copy the shortcut to the startup folder (can be opened by executing `shell:startup` in Win+R).

## Download and Compilation
The pre-compiled binaries can be found at [Releases][release] page.

The binary can be cross-compiled using MinGW-w64 as follows:

```bash
x86_64-w64-mingw32-gcc lidlock.c -o lidlock.exe -lole32 -lksguid -static -O2 -g0 -mwindows -Wall
```

For debugging, add a flag `-DDEBUG` during compiling and run the binary with a single argument which is the log file for tracking outputs:

```cmd
lidlock.exe debug.log
```

## Credits
This app is based on [`laplock`][laplock] by @dechamps and rewritten in C. The extra feature is the support of external monitors.

The license is the same as `laplock` (GPLv3):

[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

[release]: https://github.com/linusyang92/lidlock/releases
[laplock]: https://github.com/dechamps/laplock
