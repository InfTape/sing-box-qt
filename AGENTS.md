# Repository Guidelines

## Project Structure & Module Organization

This is a Qt 6/C++17 desktop application built with CMake. Main application
code lives under `src/`: `app/` wires controllers and UI startup, `core/`
handles kernel and proxy runtime logic, `coremanager/` builds the helper/service
process, `services/` contains config, subscription, rules, settings, and kernel
services, `storage/` owns SQLite-backed app state and config I/O, `system/`
contains Windows integration, and `views/` plus `widgets/` contain UI. Assets are
in `resources/`. Tests are in `test/`, grouped by feature area.

## Build, Test, and Development Commands

Configure once with your Qt path, for example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt
```

Build Release binaries:

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build --config Release --parallel
```

Run the test suite:

```powershell
C:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -C Release --output-on-failure
```

Run locally with `build\Release\sing-box-qt.exe`. The helper binary is
`build\Release\sing-box-core-manager.exe`.

## Coding Style & Naming Conventions

Use UTF-8 for all files. C++ formatting follows `.clang-format` based on Google
style: 2-space indentation, 80-column target, no include sorting, and preserved
include blocks. Name classes and files in PascalCase where a class owns the file
(`MainWindow`, `KernelService`). Use camelCase for functions and `m_` prefixes
for member fields. Keep platform-specific Windows code behind `#ifdef Q_OS_WIN`.

## Testing Guidelines

Tests use Qt Test and CTest. Add focused `*_test.cpp` files under the matching
`test/` subdirectory, and reuse helpers from `test/unit_test_shared.h` for
settings and portable data isolation. Run CTest before submitting changes. Add
coverage when touching config generation, settings persistence, proxy mode
behavior, service startup, or storage paths.

## Commit & Pull Request Guidelines

Recent history uses concise English imperative messages and occasional
Conventional Commit prefixes, such as `Add single-instance guard and stabilize
tests` or `feat: implement configuration management`. Prefer one logical change
per commit.

Pull requests should include a short summary, user-visible behavior changes,
test results, and screenshots for UI changes. Mention Windows service, UAC,
registry autostart, or data-directory impacts when relevant.

## Security & Configuration Tips

Do not commit generated `build/` output, portable `data/`, downloaded kernels, or
local SQLite databases. Treat service installation, registry autostart, and TUN
mode changes as Windows-specific behavior requiring explicit review.
