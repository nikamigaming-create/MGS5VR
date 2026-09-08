# Contributing

MGS5VR is an experimental, MIT-licensed native VR mod. Contributions to its
source, tests and documentation use that license; existing third-party notices
remain in place. All runtime integration code and build tooling live in this
repository. There is no private mod component required to build it.

## Build and validation

Use Windows x64, Visual Studio 2022 C++ tools, a Windows SDK, Git, CMake 3.24+
and Python 3. Run `./tools/build.ps1` to fetch the pinned dependencies, build
and run all six native suites. The D3D11 mailbox suite requires a hardware GPU.
GitHub-hosted CI builds every target and runs the other five suites. CI also
checks the packaged installer with authored fixtures using `tests/installer_tests.ps1`.
CI does
not launch the game or certify a headset.

Read [the architecture](docs/ARCHITECTURE.md) and
[acceptance requirements](docs/ACCEPTANCE.md) before changing render hooks.
Keep each change bounded and describe its observed behavior, validation and
remaining limitations in the pull request.

## Engine and VR evidence

- Require an exact supported executable build and validate every native hook.
- Render both eyes from one native simulation transaction with matching image,
  camera, projection and pose metadata. Do not substitute alternate frames,
  duplicated mono images or generated eyes.
- Use the game's real UI pixels, weapon state and gameplay actions. Keep UI,
  hand and weapon geometry joined to the submitted game frame.
- Distinguish unit checks, GPU tests, simulator observations and physical
  headset acceptance. A passing build is not a completed VR conversion.
- Use OpenXR actions for automated gameplay and UI tests. Do not add Windows
  focus, mouse or keyboard automation.

## Files suitable for contribution

Submit authored source, public interface contracts, numeric build adapters,
tests and documentation. Do not commit game executables or archives, extracted
assets, saves, raw disassembly, memory dumps, local account paths or credentials.
Keep private runtime evidence outside the source tree. A legally owned local
installation is required to validate the game integration.

Bug reports should include the mod revision, executable hash/version, runtime,
headset/controller model, window settings, reproduction steps and the relevant
small log excerpt with personal paths removed. Describe what was actually
visible in each eye and whether the issue needs motion to reproduce.
