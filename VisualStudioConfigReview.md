# Visual Studio Configuration Review

This document summarizes the current Visual Studio solution/project settings and highlights the changes recently applied for optimization.

## Solution (KibakoEngine.sln)
- Targets x64 only with Debug/Release configurations, which is fine for Windows-only tooling.
- Sandbox depends on engine project to ensure the static library builds first.

## Kibako2DEngine (Static Library)
- **Strengths:**
  - Uses `/W4`, `/SDL`, `/permissive-`, `/Zc:__cplusplus`, and `ConformanceMode` with warnings-as-errors for strict builds.
  - Multi-processor compilation enabled and C++20 standard selected.
  - Library output redirected to `$(SolutionDir)lib\$(Configuration)` for predictable consumption.
- **Recent improvements applied:**
  - OutDir/IntDir now point to solution-level `bin` and `intermediates` folders to keep artifacts out of the source tree.
  - Runtime library explicitly set (`MultiThreadedDebugDLL`/`MultiThreadedDLL`) and Spectre mitigation enabled.
  - Program database generation enabled for all configs, plus control-flow guard enabled in Release.
  - Public include directories exported so dependents inherit engine headers automatically.

## Kibako2DSandbox (Application)
- **Strengths:**
  - Matches warning/SDL/permissive options with the engine and consumes the engine via a project reference.
- **Recent improvements applied:**
  - Duplicated engine include paths removed; the project now relies on the engine's exported public includes.
  - OutDir/IntDir aligned with engine layout, keeping build products under solution-level folders.
  - Linker dependencies expanded to include `d3d11.lib` and `dxgi.lib`; FASTLINK enabled for Debug and `/OPT:REF /OPT:ICF /guard:cf` for Release.
  - Runtime libraries set explicitly with Spectre mitigations and PDB generation enabled.
  - `UseLibraryDependencyInputs` added to allow whole-program optimization across the static library in Release builds.

## General recommendations
- Add a shared `.props` file for common compiler/linker settings to keep the two projects in sync and ease future platform additions.
- If deploying on multiple Windows versions, pin `WindowsTargetPlatformVersion` to a specific SDK (e.g., `10.0.22621.0`) to avoid picking different SDKs on different developer machines.
- Consider generating PDBs for Release (`<DebugInformationFormat>ProgramDatabase` + `<GenerateDebugInformation>true>`) to improve post-mortem debugging.
