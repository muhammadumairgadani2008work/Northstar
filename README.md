# Northstar Personal Banking Demo

This project contains a Qt 6 Widgets banking demo and a portable Windows build.
The top-level folder is organized into a source/development folder and a
ready-to-run installation package.

## Project layout

```text
Bank/
|-- README.md
|-- Source/
|   |-- Bank.pro
|   |-- main.cpp
|   |-- mainwindow.h
|   |-- mainwindow.cpp
|   |-- mainwindow.ui
|   |-- Original/
|   |   `-- Bank-Management-system/
|   |       |-- .git/                 (hidden Git history for the reference)
|   |       |-- BANK MANAGEMENT SYSTEM
|   |       `-- README.md
|   |-- .qtcreator/
|   |   `-- Bank.pro.user
|   `-- build/
|       `-- Desktop_Qt_6_12_0_MinGW_64_bit_Debug/
|           |-- debug/
|           `-- release/
`-- Installation/
    |-- Bank.exe
    |-- account.json
    |-- qt.conf
    |-- Qt and MinGW runtime DLLs
    `-- Supporting Files/
        `-- platforms/
            `-- qwindows.dll
```

### `Source/` — development and code review

This folder contains the editable Qt project. Open `Source/Bank.pro` in Qt
Creator, select the Qt 6.12 MinGW 64-bit kit, and use **Ctrl+B** to build or
**Ctrl+R** to run.

- `Bank.pro` lists the C++ source/header files and the Qt Designer form used by
  qmake.
- `main.cpp` starts the Qt application and opens the main window.
- `mainwindow.h` declares the main window, account and transaction data, and
  the screen/action handlers.
- `mainwindow.cpp` builds the GUI, handles sign-in and account operations,
  validates input, and reads/writes account records.
- `mainwindow.ui` is the Qt Designer form. Most current screens are assembled
  programmatically in `mainwindow.cpp`.
- `Original/Bank-Management-system/` contains the original console C++ program
  and its project description. It is included as a reference and is not
  compiled into the Qt GUI. The original source file is left unchanged.
- `.qtcreator/Bank.pro.user` stores local Qt Creator kit/run settings. It is
  machine-specific; if those settings do not work on another computer, open
  `Bank.pro` and select that computer's installed Qt kit.
- `build/` contains generated Makefiles, object files, and Debug/Release
  executables. These are build outputs, not the editable source of the
  application.

The Qt GUI is a separate implementation inspired by the original program's
account and transaction features. It does not directly call the console
program.


### `Installation/` — ready-to-run Windows application

Run `Installation/Bank.exe` to open the packaged GUI without opening Qt Creator.
The package contains the Qt and MinGW runtime DLLs needed by Windows. The
`Supporting Files/platforms/qwindows.dll` plugin is kept in Qt's required
`platforms` subfolder. `qt.conf` tells Qt to look for plugins in `Supporting
Files`.

This is a portable application package, not a setup wizard. Keep the executable
and its supporting files together when copying or sharing the folder.

## Features

- Create accounts and sign in using an account number or customer name.
- Require account numbers to contain at least eight digits.
- Reject duplicate account numbers and duplicate customer names.
- View the account overview, current balance, and account details.
- Deposit and withdraw money, with checks against invalid amounts and
  insufficient funds.
- View recent activity and transaction history.
- Change the account password and sign out.
- Save multiple accounts and transaction histories as JSON.

The account-data file is named `account.json`. Each running executable uses the
file beside itself: the packaged app uses `Installation/account.json`, while
Qt Creator's Debug/Release executable uses the JSON file in that executable's
build output folder. These locations contain separate data.

If an older account file exists in the Windows AppData location, the app can
load it when no `account.json` exists beside the executable and copy its records
to the executable's folder on a successful save. Existing AppData records are
not deleted.

## Important note

This is an educational demonstration, not production banking software. Its
local JSON storage and password handling are not suitable for real financial
accounts or sensitive personal information.
