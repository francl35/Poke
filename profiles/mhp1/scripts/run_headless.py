#!/usr/bin/env python3
"""Run the MHP1 diagnostic with an independent process timeout (macOS/Linux)."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, required=True, help="Matching patched, uncompressed ISO")
    parser.add_argument("--build-dir", type=Path, default=Path("out/mhp1-headless"))
    parser.add_argument("--elf", type=Path, default=Path("profiles/mhp1/game/EBOOT.ELF"))
    parser.add_argument("--ms0", type=Path, help="Defaults to <build-dir>/ms0")
    parser.add_argument("--log", type=Path, help="Defaults to <build-dir>/boot.log")
    parser.add_argument("--renderer", action="store_true", help="Use the Vulkan backend (requires MHP1_VULKAN=ON)")
    parser.add_argument("--capture-dir", type=Path, help="Save bounded framebuffer captures here")
    parser.add_argument("--seconds", type=int, default=20)
    args = parser.parse_args()
    if args.seconds < 0:
        parser.error("--seconds must be non-negative; 0 means unlimited")
    executable = (args.build_dir / "bin/PokeFreedomDXBoot").resolve()
    if not executable.is_file():
        parser.error(f"Build PokeFreedomDXBoot first: {executable}")
    if not args.elf.is_file() or not args.iso.is_file():
        parser.error("Both --elf and --iso must name existing files")
    memory_stick = (args.ms0 or args.build_dir / "ms0").resolve()
    log_path = (args.log or args.build_dir / "boot.log").resolve()
    command = [str(executable), str(args.elf.resolve()), str(args.iso.resolve()),
               str(memory_stick), str(args.seconds)]
    log_path.parent.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    if args.renderer:
        environment["MHP1_RENDERER"] = "1"
    if args.capture_dir:
        environment["MHP1_RENDER_DIR"] = str(args.capture_dir.resolve())
    with log_path.open("wb") as output:
        try:
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                    timeout=None if args.seconds == 0 else args.seconds + 5, check=False, env=environment)
        except subprocess.TimeoutExpired:
            output.write(b"\n[mhp1-runner] External timeout: diagnostic process terminated\n")
            print(f"External timeout; diagnostic log: {log_path}")
            return 124
    print(f"Diagnostic exit {result.returncode}; log: {log_path}")
    return result.returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except OSError as error:
        print(f"Cannot run MHP1 diagnostic: {error}", file=sys.stderr)
        sys.exit(1)
