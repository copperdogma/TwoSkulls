#!/usr/bin/env python3
"""Run portable production-source checks without hardware or ESP-IDF.

Requires Python 3 and a C++17 compiler with Address/UndefinedBehaviorSanitizer.
All generated fixtures and executables live in an isolated temporary directory.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    run([sys.executable, "-m", "unittest", "discover", "-s", "scripts/tests", "-v"])
    suites = [
        ("wave", ["skull_wave"], None),
        ("catalog", ["skull_catalog", "skull_wave"], None),
        ("config", ["skull_config"], None),
        ("secondary", [], None),
        ("actuators", ["skull_actuators"], "actuator_stubs"),
        ("audio", ["skull_audio", "skull_wave"], "audio_stubs"),
        ("show", ["skull_show", "skull_catalog", "skull_wave"], "show_stubs"),
        ("storage", ["skull_storage", "skull_config"], "storage_stubs"),
    ]
    with tempfile.TemporaryDirectory(prefix="twoskulls-host-") as directory:
        temporary = Path(directory)
        for name, sources, stubs in suites:
            print(f"\n=== {name} ===", flush=True)
            executable = temporary / name
            args = shlex.split(os.environ.get("CXX", "c++")) + [
                "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-Ifirmware/main",
            ]
            if stubs:
                args += [f"-Iscripts/tests/{stubs}"]
            if name == "audio":
                args += ["-pthread", f'-DSKULL_AUDIO_SD_ROOT="{temporary / "audio-fixtures"}"']
            if name == "storage":
                args += [f'-DSKULL_STORAGE_ROOT="{temporary / "storage-fixtures"}"']
            args += [f"scripts/tests/test_skull_{name}.cpp"]
            args += [f"firmware/main/{source}.cpp" for source in sources]
            run(args + ["-o", str(executable)])
            fixtures = []
            if name == "wave":
                fixtures = [str(path) for path in sorted((ROOT / "sd_card_files/audio").glob("*.wav"))]
            elif name == "catalog":
                fixtures = [str(ROOT / "sd_card_files/audio")]
            run([str(executable)] + fixtures)
    print("\nAll host suites passed.")


if __name__ == "__main__":
    main()
