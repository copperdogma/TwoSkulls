#!/usr/bin/env python3
"""Compare production jaw commands with legacy smoothing on 20 ms WAV windows.

Read-only PCM analysis; compiles the real actuator driver against host LEDC/time
stubs. Reports commanded travel, not physical servo motion or phoneme alignment.
"""
import argparse
import array
import csv
import json
import math
from pathlib import Path
import shlex
import os
import subprocess
import sys
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]


def envelope(path):
    with wave.open(str(path), 'rb') as audio:
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (2, 2, 44100):
            raise ValueError('Expected 44.1 kHz stereo 16-bit PCM')
        samples = array.array('h', audio.readframes(audio.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    windows = [samples[i:i + 1764] for i in range(0, len(samples), 1764)]
    return [int(math.sqrt(sum(v * v for v in window) / len(window))) for window in windows]


def legacy(levels):
    amplitude, jaw = 0.0, 0
    result = []
    for rms in levels:
        amplitude = .1 * rms + .9 * amplitude
        scaled = min(amplitude * 5, 15000)
        if scaled < 1000:
            scaled = 0
        target = int(scaled * 80 / 15000)
        jaw = int(.2 * target + .8 * jaw)
        result.append(jaw)
    return result


def pulse_duty(degrees):
    return ((544 + degrees * 1856 // 180) * 65536 + 10000) // 20000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wav', nargs='+', type=Path)
    parser.add_argument('--output-directory', required=True, type=Path)
    args = parser.parse_args()
    args.output_directory.mkdir(parents=True, exist_ok=False)
    reports = []
    with tempfile.TemporaryDirectory(prefix='twoskulls-jaw-') as directory:
        temporary = Path(directory)
        executable = temporary / 'actuators'
        command = shlex.split(os.environ.get('CXX', 'c++')) + [
            '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
            '-Ifirmware/main', '-Iscripts/tests/actuator_stubs',
            'scripts/tests/test_skull_actuators.cpp', 'firmware/main/skull_actuators.cpp',
            '-o', str(executable)]
        subprocess.run(command, cwd=ROOT, check=True)
        for index, path in enumerate(args.wav):
            levels = envelope(path)
            inputs = temporary / 'rms.txt'
            inputs.write_text('\n'.join(map(str, levels)) + '\n')
            output = subprocess.check_output([str(executable), str(inputs)], text=True)
            duties = [int(line) for line in output.splitlines()]
            assert len(duties) == len(levels)
            inverse = {pulse_duty(angle): angle for angle in range(81)}
            current = [inverse[duty] for duty in duties]  # Reject out-of-range commands.
            previous = legacy(levels)
            csv_path = args.output_directory / f'{index}-jaw.csv'
            with csv_path.open('w') as stream:
                writer = csv.writer(stream)
                writer.writerow(['seconds', 'rms', 'legacy_degrees', 'new_degrees'])
                writer.writerows((round(i * .02, 3), rms, old, new)
                                 for i, (rms, old, new) in enumerate(zip(levels, previous, current)))
            reports.append({'wav': str(path), 'windows': len(levels),
                            'rms_peak': max(levels), 'legacy_peak_degrees': max(previous),
                            'new_peak_degrees': max(current),
                            'legacy_total_travel_degrees': sum(abs(b-a) for a,b in zip(previous, previous[1:])),
                            'new_total_travel_degrees': sum(abs(b-a) for a,b in zip(current, current[1:])),
                            'trace': str(csv_path)})
    result = {'scope': 'Host commanded motion only; no hardware or syllable recognition', 'files': reports}
    (args.output_directory / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
