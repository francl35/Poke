# Poke

**Poke** is an experimental native recompilation project focused on **Monster Hunter Portable 2nd G** (MHP2G, known internationally as *Monster Hunter Freedom Unite*) for PSP.

Poke is a **derivative of [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo)**. The PSP runtime, recompilation framework, and substantial portions of the host implementation build on work done by Yakumo's original contributors. **Poke is not the original Yakumo project, is not maintained by the Yakumo team, and is not an official successor.** Please refer to the upstream project for its own releases, development, and support. Credit for the original work belongs to its respective authors and contributors.

This fork focuses on adapting that technology to MHP2G. The inherited MHP3rd profile remains in the repository; its presence does not mean that Poke's MHP2G build is a release of the original Yakumo project.

## Current status

**Work in progress — not a general release.**

- The MHP2G executable has been analyzed and its main code generated as native C++.
- The Linux Vulkan build has booted successfully and village gameplay has been tested.
- Quest, combat, zone-transition, and overlay compatibility testing is still in progress.
- The MHP2G CMake target and executable are both named `Poke`.
- Some guest code can fall back to an interpreter; the project is not exclusively ahead-of-time execution.

These observations are development test results, not a claim of full game compatibility or platform support.

## Game files and legal notice

**No copyrighted game content is included in this repository.** You must supply files from your own legally obtained copy of *Monster Hunter Portable 2nd G*. Game disc images, decrypted executables, extracted assets, game-derived generated C++ code, and compiled game overlays are not distributed here and must not be committed.

Poke is an unofficial fan-made compatibility project and is not affiliated with, authorized, endorsed, or sponsored by CAPCOM, Sony, or their affiliates. *Monster Hunter*, *Monster Hunter Portable 2nd G*, *Monster Hunter Freedom Unite*, PSP, and related names and assets belong to their respective owners. Users are responsible for complying with applicable laws when preparing and using their own game files.

## Building MHP2G on Linux (development)

Requirements include a C++20 toolchain, CMake, Python 3, SDL3, Vulkan development files, and shader compilation tools. The MHP2G renderer build also uses FFmpeg for media support. See the CMake configuration and existing build documentation for additional dependencies.

Configure and build the development tools and renderer:

```bash
/usr/bin/cmake -S . -B out/mhp2g-renderer -G "Unix Makefiles" \
  -DPSPRECOMP_PROFILE=mhp2g -DMHP2G_RENDERER=ON
/usr/bin/cmake --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

Generate the main ahead-of-time code locally from your **own decrypted MHP2G `EBOOT.ELF`**:

```bash
profiles/mhp2g/scripts/generate.sh \
  "$PWD/out/mhp2g-renderer" \
  "/path/to/your/EBOOT.ELF"
```

Rebuild the executable after generation:

```bash
/usr/bin/cmake --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

Run it with the directory containing your local game files:

```bash
./out/mhp2g-renderer/bin/Poke "/path/to/your/game-directory"
```

**Overlay generation is not yet documented as a verified, standalone Poke workflow.** The profile includes an overlay build script, but its game-image path and output assumptions are being adapted. Until that is completed, some code may execute through the interpreter. Do not copy or commit generated game code or overlay libraries into tracked source directories.

## Project layout

- `profiles/mhp2g/` — MHP2G host code, tools, shaders, scripts, and tests.
- `profiles/mhp3rd/` — inherited MHP3rd profile from the upstream project.
- `include/psprecomp/`, `src/`, `tools/` — shared PSP runtime and recompilation framework.
- `docs/` — project documentation; some inherited documents still describe upstream MHP3rd behavior and should not be treated as verified MHP2G instructions.

## Credits and licensing

Poke is based on **[TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo)**, an independent project developed by contributors whom the maintainer of this fork does not represent. Thank you to the Yakumo team and all upstream contributors for the original runtime, tooling, and implementation work.

The upstream project is distributed under the **MIT License**. Retain the original copyright and permission notices in `LICENSE` and any applicable source files. Third-party components may have their own licenses and notices; consult their accompanying license files. The software license does **not** grant rights to distribute CAPCOM or Sony game content.

For upstream Yakumo's own documentation and releases, visit [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo). For this fork, use [francl35/Poke](https://github.com/francl35/Poke).
