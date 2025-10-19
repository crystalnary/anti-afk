# antiafk

A lightweight anti-AFK tool for Windows applications.

## About

Inspired by [Agzes/AntiAFK-RBX](https://github.com/Agzes/AntiAFK-RBX) but reimagined as a simple command-line tool. While the original works well, it comes with 3000+ lines including a full GUI system, update checkers, announcement systems, tutorials, and extensive UI theming.

This version strips it down to the core functionality in ~200 lines.

## Features

* **Lightweight** - Pure CLI, no system tray or GUI overhead

* **Process agnostic** - Works with any Windows application

* **Simple** - No configuration files or registry entries

* **Focused** - Does anti-AFK and nothing else

## Usage

```bash
antiafk <process_name> [options]
```

### Options

* `-i, --interval <seconds>` - Set interval between actions (default: 540)

* `-a, --action <type>` - Set action type: `space`, `wasd`, or `zoom` (default: space)

* `-h, --help` - Show help

### Examples

```bash
# Basic usage
antiafk notepad.exe

# Custom interval (5 minutes)
antiafk game.exe -i 300

# WASD movement pattern
antiafk "My Game.exe" -a wasd

# Full options
antiafk RobloxPlayerBeta.exe -i 600 -a zoom
```

## Building

### Requirements

* Windows

* CMake 3.10+

* C++17 compiler (MSVC or MinGW)

* spdlog headers (header-only logging library)

### Build Steps

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Or with MSVC directly:

```bash
cl main.cpp /std:c++17 /I include /link user32.lib kernel32.lib
```

Or with MinGW directly:
```bash
g++ main.cpp -std=c++17 -I include -O2 -luser32 -lkernel32 -o antiafk.exe
```

## How It Works

1. Finds all windows belonging to the target process

2. Every N seconds, brings each window to foreground

3. Sends configured key presses

4. Restores your previous window focus

No background services, no persistent settings, just runs when you need it.

## Why This Exists

The original AntiAFK-RBX includes a lot of features that aren't very necessary for the core anti-AFK functionality: custom drawn UI elements, splash screens, multi-page tutorials, registry-based settings persistence, update checking systems, and announcement frameworks. While these features create a polished experience, they also add significant complexity and bloat.

This tool takes a different approach: just the anti-AFK logic in a simple CLI. No GUI code, no Win32 message loops, no custom icon drawing, no registry access.

## License

MIT
