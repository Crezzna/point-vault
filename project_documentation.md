# Win32 File Integrity Monitor & Secure Vault (PointVault)

## Overview
`PointVault` is a native, lightweight systems-level utility developed in C++ designed for Microsoft Windows environments. It provides real-time file integrity monitoring (FIM) and automated backup/vault relocation mechanisms. By leveraging native Win32 APIs, the application operates with minimal resource overhead, detecting unauthorized or targeted modifications, deletions, or renames, and ensuring data preservation via an in-memory shadow caching architecture.

---

## Key Features

* **Event-Driven Directory Monitoring:** Utilizes `ReadDirectoryChangesW` to asynchronously track file-system events (`FILE_ACTION_MODIFIED`, `FILE_ACTION_REMOVED`, `FILE_ACTION_RENAMED`) at the kernel level without relying on CPU-intensive polling.
* **In-Memory Shadow Caching:** Implements a RAM-based buffer (`unordered_map<wstring, vector<char>>`) upon initialization. This safeguards against total data loss by enabling immediate file reconstruction even if the target file is forcefully deleted or unlinked.
* **Native GUI Integration:** Employs Windows Shell Common Item Dialog interfaces (`IFileOpenDialog`) to provide a seamless file-selection and directory-routing user experience.
* **Robust Logging Architecture:** Maintains a persistent, timestamped operational log (`debug.log`) for auditing and debugging lifecycle events.
* **Graceful Lifecycle Management:** Intercepts console and system control signals (`SetConsoleCtrlHandler`) to execute safe cleanup procedures prior to termination.

---

## Technical Architecture & Workflow

1. **Initialization & Target Acquisition:**
   * Upon execution, the application prompts the user via native Windows dialogs to select up to ten target files and designate a secure destination (vault) directory.
2. **Pre-Execution Shadow Caching:**
   * Target files are read in binary mode (`ios::binary | ios::ate`) and stored securely in process memory to serve as a fail-safe fallback.
3. **Handle Binding & Event Listening:**
   * The application opens asynchronous directory handles (`FILE_FLAG_BACKUP_SEMANTICS`) for the respective parent directories and enters an event-waiting loop.
4. **Trigger Response & Relocation:**
   * Upon detecting a monitored file-system change, the engine attempts to perform an atomic file relocation (`MoveFileExW`) to the designated vault path.
   * If the physical file has been purged or deleted, the system reconstructs the file from the RAM cache directly into the secure vault.
5. **Audit Logging:**
   * All state transitions, success codes, and error markers (`GetLastError()`) are logged with precise system timestamps.

---

## Technical Specifications

* **Language:** C++17
* **Target Operating System:** Microsoft Windows (7 / 10 / 11)
* **API Dependencies:** Win32 Native API (`windows.h`), Windows Shell COM (`shobjidl.h`), C++ Standard Filesystem (`<filesystem>`)

---

## Compilation Instructions

To compile the source code manually using **MinGW-w64** via Command Prompt, run the following command:

```bash
g++ -std=c++17 -O3 point_2.cpp -o PointVault.exe -lole32 -lshell32
```

For **Microsoft Visual Studio (MSVC)** Developer Command Prompt:

```cmd
cl /EHsc /std:c++17 /O2 point_2.cpp /link ole32.lib shell32.lib
```

---

## Usage Guidelines

1. Execute the compiled binary (`PointVault.exe`).
2. Select the files you wish to monitor using the interactive file dialog interface.
3. Choose a secure target destination folder (*vault*) for relocated or reconstructed assets.
4. The system will detach the console interface and monitor the targeted file paths in the background.

---

## Disclaimer
This software is developed strictly for educational, systems-engineering, and defensive security research purposes. The author assumes no liability for any misuse or unintended data manipulation resulting from the application of Win32 system APIs.