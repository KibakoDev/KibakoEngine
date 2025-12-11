# KibakoEngine

KibakoEngine is a 2D engine built as a static library with a sample sandbox application. The repository is organized to keep engine code reusable while still making it easy to iterate on the sandbox.

## Project layout
- `Kibako2DEngine/` – engine static library source and headers under `src/` and `include/`.
- `Kibako2DSandbox/` – sample application demonstrating the engine.
- `third_party/` – vendored headers such as `stb_image.h`, shared through `Build/ThirdParty.props`.
- `Build/` – shared MSBuild property sheets.
- Build outputs: `lib/<Platform>/<Configuration>/` for engine libraries and `bin/<Platform>/<Configuration>/` for sandbox executables with intermediates in `build/<Project>/<Platform>/<Configuration>/`.

## Building
1. Install Visual Studio 2022 with the Desktop development with C++ workload and the Windows 10+ SDK.
2. Open `KibakoEngine.sln` and select the desired configuration (`Debug|x64` or `Release|x64`).
3. Build the solution. The sandbox (`Kibako2DSandbox`) depends on the engine (`Kibako2DEngine`) and will link against `lib/<Platform>/<Configuration>/Kibako2DEngine.lib` automatically.

## Running the sandbox
After building, run `bin/<Platform>/<Configuration>/Kibako2DSandbox.exe`. Assets referenced by the sandbox live under `Kibako2DSandbox/assets` and are resolved relative to the executable's working directory.

## Adding third-party headers
Place additional single-header libraries under `third_party/` and they will be available to both projects through the shared `Build/ThirdParty.props` include path.
