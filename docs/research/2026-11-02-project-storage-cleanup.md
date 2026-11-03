# Project storage cleanup — November 2, 2026

Cam requested space recovery after landing the HA/jaw feature. Main/source and
unrelated worktree edits were preserved. No skull or Mac Mini service was changed.

## Completed

- Removed generated `firmware/build/` trees from the `jaw-articulation` and
  `onboard-sd-migration` worktrees: about 1.44 GiB allocated before removal.
  Before removal, exact binaries, ELF/debug maps, build metadata/configuration
  and logs were archived and independently SHA256-verified. The two protected
  archives total about 66 MiB. Managed SDK components, dependency locks and
  sdkconfig were retained; the next native build will regenerate its outputs.
- Losslessly gzip-compressed 23 historical ELF/map files across project build
  and validation folders: 1,228,290,384 bytes became 416,888,058 bytes. Each
  decompressed SHA256 matched before removing the uncompressed original. Raw
  serial captures, board photos, firmware binaries and private backups remain.
- Removed only retired laptop containers `twoskulls-story007-ha` and
  `twoskulls-story007-mqtt`, plus their two unshared HA/Mosquitto images. Docker
  image accounting fell from about 10.98 GB to 8.64 GB. Preserved both bind-mounted
  runtime trees, MQTT volumes, cold migration archives, credentials and logs.
  Protected container definitions and exact image digests permit deliberate
  recreation. All 23 pre-existing unrelated containers were verified present.
- Docker was initially stopped and started for this maintenance. Unrelated
  projects began running containers during cleanup, so it was left running
  rather than interrupting their work. No global prune or volume deletion.

Available host space rose from roughly 3.3 GiB to 6.8 GiB during the work; other
projects were concurrently active, so this is an observed host reading rather
than an isolated attribution of every reclaimed byte. Docker's allocated VM disk
also shrank from about 18 GiB to 15 GiB. Do not confuse its logical maximum with
physical allocation.

## Where retained material lives

Under each native worktree:

`build/validation/cleanup-20261102/native-build-release-and-debug.tar.gz`

Each has `preserved-build-manifest.json` with per-file SHA256. To inspect or
recover an old build, extract into a new scratch directory, not a current build
tree. These archives preserve release/debug artifacts; they are not full object
caches. Use the pinned firmware build instructions for a new compilation.

The `jaw-articulation` worktree's same cleanup directory also holds receipts,
the lossless compression manifest (original path/hash/time and `.gz` path),
private retired-container definitions/logs, and public image digest metadata.
Use `gzip -dk <file>.elf.gz` or `<file>.map.gz` to recreate an individual debug
file while retaining its compressed copy. Verify against the recorded SHA256
when using historical evidence. None of these private receipts or captures is
committed to Git.

The dirty `onboard-sd-migration` checkout's hardware/readme and untracked research
were left unchanged; only generated build data and lossless debug representations
were cleaned. No worktree or branch was deleted.

## Method and reference

General problem: deleting Docker objects does not necessarily immediately return
all guest free space to the Mac. The official
[Docker Desktop Mac disk-space guidance](https://docs.docker.com/desktop/troubleshoot-and-support/faqs/macfaqs/)
recommends measuring image/container usage and actual disk-image allocation.
Adopt targeted removal of verified retired objects, then measure host free space
and allocated VM disk. Automatic reclamation returned host space here; no manual
VM resize, raw-file deletion or privileged reclaim helper was needed. Remaining
reclaimable Docker objects belong to other projects and were not assessed for
deletion as part of this task.
