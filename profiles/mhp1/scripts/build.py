#!/usr/bin/env python3
"""Configure/build MHP1 using audited release presets. Does not generate game code."""
import argparse
import json
import os
import queue
import shutil
import signal
import threading
import time
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def process_snapshot(child_pid=None):
    """Read-only process status; candidates are not asserted to hold a lock."""
    try:
        result = subprocess.run(
            ['ps', '-axo', 'pid=,ppid=,stat=,etime=,pcpu=,comm='],
            capture_output=True, text=True, timeout=3, check=False)
        rows = []
        for line in result.stdout.splitlines():
            fields = line.strip().split(None, 5)
            if len(fields) == 6:
                try:
                    rows.append((int(fields[0]), int(fields[1]), fields[2:]))
                except ValueError:
                    pass
        owned = {child_pid} if child_pid else set()
        changed = True
        while changed:
            previous = len(owned)
            owned.update(pid for pid, parent, _ in rows if parent in owned)
            changed = len(owned) != previous
        selected = [row for row in rows if row[0] in owned]
        candidates = [row for row in rows if row[0] not in owned and
                      any(word in row[2][-1].lower() for word in
                          ('cmake', 'ninja', 'cc1', 'clang', 'gmake', 'flock', 'curl', 'ffmpeg'))]
        print('  PID PPID STATE ELAPSED CPU% COMMAND', flush=True)
        for pid, parent, fields in selected[:12]:
            print('  ' + ' '.join([str(pid), str(parent), *fields]), flush=True)
        if candidates:
            print('  Other build-related processes (possible competitors, not proven blockers):', flush=True)
            for pid, parent, fields in candidates[:6]:
                print('  ' + ' '.join([str(pid), str(parent), *fields]), flush=True)
        if not selected and not candidates:
            print('  No matching processes visible in this environment.', flush=True)
        commands = ' '.join(row[2][-1].lower() for row in selected)
        if any(x in commands for x in ('cc1', 'clang', 'gmake')):
            print('  Compiler/make process present; a large generated unit or bundled dependency may be working.', flush=True)
        elif 'ninja' in commands or 'python' in commands:
            print('  Ninja/wrapper present: may be working or waiting for a lock; status alone cannot prove which.', flush=True)
        else:
            print('  No compiler child visible. CMake may be scanning, fetching, or blocked; use --trace-configure.', flush=True)
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f'  Cannot inspect processes: {error}', flush=True)


def stop_process_tree(process):
    if process.poll() is not None:
        return
    # Own session on POSIX: interrupt the build and descendants, not other builds.
    for sig, timeout in ((signal.SIGINT, 4), (signal.SIGTERM, 3), (signal.SIGKILL, 2)):
        try:
            os.killpg(process.pid, sig)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=timeout)
            return
        except subprocess.TimeoutExpired:
            pass


def command(args, env, dry=False, capture=False, stage='command', log=None, interval=15):
    args = [str(x) for x in args]
    print(f'[{stage}] ' + shlex.join(args), flush=True)
    if log:
        print(f'[{stage}] log: {log}', flush=True)
    if dry:
        return ''
    if log:
        log.parent.mkdir(parents=True, exist_ok=True)
    output_file = log.open('w', encoding='utf-8') if log else None
    process = None
    try:
        if output_file:
            output_file.write('+ ' + shlex.join(args) + '\n')
            output_file.flush()
        process = subprocess.Popen(args, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, errors='replace',
                                   bufsize=1, start_new_session=True)
        events = queue.Queue()
        def read_output():
            try:
                for line in process.stdout:
                    events.put(line)
            finally:
                events.put(None)
        threading.Thread(target=read_output, daemon=True).start()
        start = last_output = last_report = time.monotonic()
        captured = []
        eof = False
        print(f'[{stage}] PID {process.pid}; status every {interval}s. Ctrl+C cancels this subprocess group.', flush=True)
        while not eof or process.poll() is None:
            try:
                item = events.get(timeout=0.2)
                if item is None:
                    eof = True
                else:
                    last_output = time.monotonic()
                    print(item, end='', flush=True)
                    if output_file:
                        output_file.write(item)
                        output_file.flush()
                    if capture:
                        captured.append(item)
            except queue.Empty:
                pass
            now = time.monotonic()
            if now - last_report >= interval:
                print(f'[{stage}] elapsed {now-start:.0f}s; no output for {now-last_output:.0f}s; '
                      f'PID {process.pid}. Waiting does not necessarily mean a hang.', flush=True)
                process_snapshot(process.pid)
                last_report = now
        status = process.wait()
        print(f'[{stage}] exit {status} after {time.monotonic()-start:.1f}s', flush=True)
        if status:
            raise subprocess.CalledProcessError(status, args)
        return ''.join(captured)
    except KeyboardInterrupt:
        print(f'\n[{stage}] cancelling subprocess group; retaining cache and logs.', flush=True)
        if process:
            stop_process_tree(process)
        raise
    finally:
        if process and process.stdout:
            process.stdout.close()
        if output_file:
            output_file.close()


def read_cache(path):
    entries = {}
    if path.is_file():
        for line in path.read_text().splitlines():
            if line.startswith(('#', '//')) or '=' not in line or ':' not in line:
                continue
            key, value = line.split('=', 1)
            entries[key.split(':', 1)[0]] = value
    return entries


def ninja_wrapper_flags(build, entries):
    if 'Ninja' not in entries.get('CMAKE_GENERATOR', ''):
        return []
    make = entries.get('CMAKE_MAKE_PROGRAM', '')
    real = entries.get('PSPRECOMP_REAL_NINJA', '')
    expected = str(build / 'ninja-locked')
    # Avoid the alias/self-wrapper recursion caused by /home vs /run/host paths.
    mismatch = Path(make).name == 'ninja-locked' and make != expected
    recursion = Path(real).name == 'ninja-locked'
    if not mismatch and not recursion:
        return []
    ninja = next((shutil.which(x) for x in ('ninja-build', 'ninja') if shutil.which(x)), None)
    if not ninja or Path(ninja).resolve().name == 'ninja-locked':
        raise ValueError('Ninja wrapper mismatch detected. Install/find real ninja or ninja-build before configuring.')
    print('[preflight] Ninja wrapper alias/recursion risk detected.', flush=True)
    print(f'[preflight] cached wrapper={make}; cached real Ninja={real}', flush=True)
    print(f'[preflight] selecting actual Ninja {ninja}; locking remains enabled.', flush=True)
    return ['-DCMAKE_MAKE_PROGRAM=' + ninja, '-DPSPRECOMP_REAL_NINJA=' + ninja]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, help='Reuse a working cache/generator; defaults to a new preset directory')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--overlays', action='store_true', help='Also build every generated overlay metadata target')
    parser.add_argument('--status-interval', type=int, default=15, help='Seconds between elapsed/process reports')
    parser.add_argument('--trace-configure', action='store_true', help='Write detailed CMake trace; can be large')
    parser.add_argument('--status', action='store_true', help='Show build-related processes without building')
    parser.add_argument('--dry-run', action='store_true', help='Print commands without configuring or building')
    args = parser.parse_args()
    if args.status:
        process_snapshot()
        return 0
    if args.status_interval < 1:
        parser.error("--status-interval must be positive")
    if args.jobs < 1 or (args.configure_only and args.build_only):
        parser.error('Use positive --jobs; configure-only and build-only are mutually exclusive')
    platform = {'linux': 'linux', 'darwin': 'macos'}.get(sys.platform)
    if not platform:
        parser.error('Supported hosts: Linux and macOS')
    preset = 'mhp1-' + platform + '-release'
    build = (args.build_dir or Path('out') / preset)
    if not build.is_absolute():
        build = ROOT / build
    build = build.resolve()
    env = os.environ.copy()
    entries = read_cache(build / 'CMakeCache.txt')
    print(f'[preflight] repository: {ROOT}\n[preflight] build: {build}', flush=True)
    print(f'[preflight] cmake executable: {shutil.which("cmake")}', flush=True)
    for key in ('CMAKE_HOME_DIRECTORY', 'CMAKE_GENERATOR', 'CMAKE_MAKE_PROGRAM',
                'PSPRECOMP_REAL_NINJA', 'PSPRECOMP_BUILD_LOCK'):
        if key in entries:
            print(f'[preflight] {key}={entries[key]}', flush=True)
    extra = ninja_wrapper_flags(build, entries)
    if args.build_only and extra:
        parser.error('Ninja wrapper needs repair: run once without --build-only to reconfigure safely.')
    if platform == 'macos':
        if args.dry_run:
            print('macOS: CC/CXX and SDKROOT will be resolved using xcrun.')
        else:
            def xcrun(*parts):
                return subprocess.check_output(['xcrun', *parts], text=True).strip()
            env['CC'] = xcrun('--find', 'clang')
            env['CXX'] = xcrun('--find', 'clang++')
            env['SDKROOT'] = xcrun('--sdk', 'macosx', '--show-sdk-path')
            extra += ['-DCMAKE_C_COMPILER=' + env['CC'],
                     '-DCMAKE_CXX_COMPILER=' + env['CXX'],
                     '-DCMAKE_OSX_SYSROOT=' + env['SDKROOT']]
    if not args.build_only:
        if (build / 'CMakeCache.txt').is_file():
            cache = (build / 'CMakeCache.txt').read_text()
            if 'PSPRECOMP_PROFILE:STRING=mhp2g' in cache:
                parser.error('Refusing to reuse an MHP2G build directory')
            data = json.loads((ROOT / 'CMakePresets.json').read_text())
            variables = next(x for x in data['configurePresets'] if x['name'] == 'mhp1-release-base')['cacheVariables']
            flags = ['-D' + key + '=' + ('ON' if val is True else 'OFF' if val is False else str(val))
                     for key, val in variables.items()]
            # No -G here: preserve the existing generator and dependency paths.
            configure = ['cmake', '-S', ROOT, '-B', build, *flags, *extra]
        else:
            configure = ['cmake', '--preset', preset, '-B', build, *extra]
        if args.trace_configure:
            if not args.dry_run:
                build.mkdir(parents=True, exist_ok=True)
            trace = build / 'mhp1-configure-trace.log'
            configure += ['--trace-expand', '--trace-redirect=' + str(trace)]
            print(f'[configure] detailed trace: {trace}', flush=True)
        command(configure, env, args.dry_run, stage='configure',
                log=build / 'mhp1-configure.log', interval=args.status_interval)
    if args.configure_only:
        return 0
    command(['cmake', '--build', build, '--target', 'PokeFreedomDXBoot',
             '--parallel', args.jobs], env, args.dry_run, stage='build',
            log=build / 'mhp1-build.log', interval=args.status_interval)
    if args.overlays:
        # Matches the previously used overlay_<directory> target convention.
        metadata = sorted((ROOT / 'profiles/mhp1/overlays').glob('*/meta.txt'))
        if not metadata:
            parser.error('No generated overlay metadata. Extract/generate overlays using the existing procedure first.')
        targets = ['overlay_' + item.parent.name for item in metadata]
        print(f'Overlay metadata targets: {len(targets)}')
        command(['cmake', '--build', build, '--target', *targets,
                 '--parallel', args.jobs], env, args.dry_run, stage='overlays',
                log=build / 'mhp1-overlays-build.log', interval=args.status_interval)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f'MHP1 build failed: {error}', file=sys.stderr)
        raise SystemExit(1)
