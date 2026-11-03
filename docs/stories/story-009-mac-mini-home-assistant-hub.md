# Story 009 — Permanent Mac Mini Home Assistant hub

Date: November 2, 2026.
Status: Live — Mini HA/MQTT and Jeff dashboard/log/playback checks passed; host reboot, power-loss and phone reconnection remain separate qualification.

## Objective

Cam selected the always-on Mac Mini as the primary Home Assistant hub for the
whole house, including Halloween props. Coordinate with its existing **Set up
Home Assistant** chat, `01a19d57-a9b4-7f32-8af7-c8af0474d624`, on host
`remote-control:env_e_6a70dd7f7b38832a941b1c35b41b8bbd`.
The Mini-side owner maintains its host runbook in OpenAI Bishop; TwoSkulls owns
the donor configuration, prop endpoint changes, dashboard/log verification and
the reusable [integration guide](../home-assistant-prop-integration.md).

## Acceptance and migration design

- Preserve the HA instance coherently: users/auth, device/entity registries,
  default grouped Halloween dashboard, country/time zone, mobile registration,
  custom diagnostics backend, report cache and required keys.
- Preserve broker authentication; use durable, bounded persistence on the
  permanent host. Source broker intentionally has persistence disabled.
- Verify physical disk/update headroom and peak runtime resources before a large
  pull. Reuse existing Colima if supportable; do not create a competing VM or
  interfere with existing household services. No native Python Core fallback.
- Keep the same HA version initially (2026.10.4), then separate later upgrades
  from migration. Set a whole-house instance name while keeping Halloween as a
  prop dashboard. Do not enroll unrelated devices or expose public access.
- Only one active restored HA instance during cutover. Keep laptop rollback
  data intact until destination validation succeeds. Update HA's broker entry
  and Jeff's configured broker endpoint without changing his SD Wi-Fi settings.
- Verify target reachability, persisted dashboard, actual selection/play/Idle,
  exact SD report retrieval, offline/reconnect, startup supervision, bounded
  logs/history/backups and a documented restore/update path.
- Distinguish verified service restart from an actual host reboot/cold-power
  test. Respect the Mini's existing reboot-resilience procedure and other services.

## Initial audit and evidence

Mini owner reports Intel macOS 15.7.7, 8 GiB RAM, approximately 5.6–5.7 GiB
physical internal disk free, existing Colima 2 GiB / 2 vCPU with five running
containers. The guest has spare logical disk capacity, which is not physical
host free space. The existing host storage monitor warns below 10% physical free; it does not
prohibit deployment. The Mini owner initially overstated this as a hard gate and
corrected it. Initial 10.2/13.2 GiB reclaim targets preserved that recommended
margin, rather than describing HA minimum requirements. Cam explicitly selected
this host despite tight space. A bounded current-runtime image pull/installation
preparation with a 2 GiB physical-free cutoff is now authorized; capacity must be
measured during actual growth without disrupting existing services.
Do not prune Docker, compact its live VM, move Books data or use SMB for live
SQLite/VM disks merely to bypass this gate.

Mini LAN last verified `192.168.1.80`, hostname `The-Octagon.local`, account
`occam`. DHCP reservation is not yet verified. Existing SSH is enabled; laptop
matched the Mini's ED25519 host fingerprint through the independently reported
pin. Existing noninteractive authentication failed. A temporary public key is
prepared, not installed: from laptop IP only, no forwarding/PTY/shell, forced
write to one private incoming archive, refuses overwrite, expires after two
hours and is removed after a verified transfer. Mini's authentication policy
requires Cam's explicit approval; the request is pending in its own chat.
No secret values are passed through chat or Git.

The Mini already disables system sleep and has automatic login, but its existing
runbook records failure to restart after AC loss despite autorestart configured.
Do not claim unattended power-loss recovery or silently alter that policy.

Source cold backup was made after graceful HA stop, then source HA restarted:

- Archive: private ignored `build/validation/story-007-ha/mac-mini-migration/laptop-migration.tar.gz`.
- Size: 2,886,445 bytes; SHA256
  `b2ffd207f76fb7f015eb59fbeca2eb9de022529f0a8ea90befb34a4bf75e0836`.
- SQLite integrity check passed; nine critical configuration/auth/registry/
  dashboard files verified against archived byte hashes; both report caches included.
- Full configuration, broker password data and private auth helper files are in
  this mode-600 archive. It must remain protected and excluded from Git.
- A separate 24,981-byte source/guide archive is included, because this worktree's
  current HA implementation and guide have not been committed/pushed yet.
- Source broker has no persistent message database. Retained discovery/readiness
  should be refreshed by device reconnection, not treated as live solely because
  it was copied from an old online snapshot.

Source baseline is connected/idle Jeff with two saved reports. A minimum internal
heap reading of 47 bytes was observed after earlier playback. This is a distinct
firmware resource concern, not evidence of a server-migration fault; preserve it
and do not claim exhaustive active-audio headroom or add firmware features here.

Mini audit/runbook was committed/pushed in its repository through `523d109` on
`codex/home-assistant-hub`. No destination service activation, credential change,
large image pull or completed migration is claimed at this checkpoint.

## Research decision

General problem: consistent state migration between server runtimes, with
protected authentication/registries and a separately owned MQTT broker.
Use a gracefully quiesced coherent configuration archive for a same-version
Container move; official HA backup/restore remains the supported migration path
if the chosen target changes to HA OS. A Container backup does not include the
external broker; preserve that separately. Keep source rollback and avoid
simultaneously active copies. Use pinned SSH transport for the private artifact.

Primary sources:

- [HA backups, contents and restore](https://www.home-assistant.io/common-tasks/general/backups/)
- [Official macOS HA OS VM route](https://www.home-assistant.io/installation/macos/)
- [OpenSSH restricted forced-command keys](https://man.openbsd.org/sshd.8)

The measured backup/hash/SQLite checks demonstrate source consistency. Runtime
choice, host capacity, startup and full target behavior still require their own
local proof. Record the final cutover endpoint, validations and remaining human
actions here and update the shared guide when those become true.

### Corrected capacity framing

The Mini's final read-only volume inventory found no attached local external SSD;
Movies, TV, Sharing Is Fun and Data are SMB/NAS mounts. Do not place a new live
VM/database there as a workaround. Exact image metadata: HA amd64 compressed
659,354,540 bytes; Mosquitto 2.0.22 compressed 7,717,986 bytes. The earlier expanded
and two-generation estimates were provisional. Laptop idle measurements were
366.1 MiB HA and 2.77 MiB MQTT; target startup peaks still need actual proof.
Existing Books LaunchAgent starts Colima at login and checks it every five minutes;
new containers can use this owner without restarting/reconfiguring the live VM.
A prepared bounded deployment is underway independently of the transfer approval.
Colima multicast discovery for broader HomeKit/Matter integrations remains a
separate qualification; a successful MQTT move does not prove every home protocol.

### Transfer artifact finalized before upload

The original cold archive remains preserved as `laptop-migration.cold-original.tar.gz`.
The upload archive additionally includes a 44-file public implementation/source
snapshot against base `f2146d6a08acdf844634eaddb0f95696f4bb766a`, because the
uncommitted HA firmware/guide/receiver work cannot be reconstructed from a main
clone alone. Cold HA contents are unchanged. Final upload is 3,009,322 bytes,
SHA256 `7575e4eabd85b2ef23fbbe69a7a5bad3bec3a8b28cfeff947ded1700b13b1797`.
The whole archive remains private mode600. No transfer has occurred yet.

### Bounded image preparation result

The Mini owner reports both pinned amd64 images downloaded successfully using the
existing Colima runtime. Lowest sampled physical host free space was approximately
3.28 GiB, above the explicit 2 GiB cutoff; existing service checks passed during
the pull. No HA/MQTT container was created or started at this stage. The former
10% recommendation is still a storage warning, not a migration prohibition.
Service-file validation and protected transfer/cutover remain pending. The
laptop's live service remains running and rollback is preserved.

### Prepared service files and remaining human access step

Mini-side Compose/configuration is validated and pushed through `b2de8fc`, with
container memory bounds, rotated logs, persistence and readiness checks. All five
existing containers kept their restart counts; no VM/host restart or new HA/MQTT
container activation occurred. Subsequent host allocation reduced physical free
space to approximately 2.15–2.16 GiB, with swap at 4 GiB. Startup therefore still
needs a fresh measured capacity/memory check and monitored abort/rollback; a
successful image pull is not runtime acceptance. Source broker was freshly
confirmed as Mosquitto2.1.2; the Mini owner is reconciling its initial2.0.22 pin
before activation. Source HA remains the live instance. The existing restricted
SSH upload approval in the Mini chat is still required before private transfer.

Latest Mini preflight then recovered to approximately 3.13 GiB physical free as
swap allocation fell to 3 GiB; guest available memory was 1,111 MiB and Books
health passed. This supports a monitored startup attempt after protected transfer
and a fresh check, without asserting a fixed additional reclamation requirement.
Removing a Docker image may not immediately reclaim host physical sparse-disk
allocation; do not promise it as an instant physical-space rollback or compact the
shared live VM. Transfer approval remains pending; no target activation yet.

## Live cutover and prop acceptance

- Cam approved the upload key directly in the Mini chat. The prepared archive
  transferred over pinned encrypted SSH. Mini verified byte count/SHA, nine
  critical hashes, both cache hashes and SQLite integrity, then revoked the exact
  key. Laptop independently observed authentication denial and removed its
  private transfer key. No persistent new SSH access remains.
- Source dashboard/config/registry bytes were unchanged since backup. Laptop HA
  stopped cleanly before target activation. The Mini restored name **Home** and
  local broker routing while preserving users/auth, mobile registration, entity/
  device UUIDs, default dashboard, diagnostics component and cache.
- Target `home-assistant` (2026.10.4) and `home-mosquitto` (2.1.2-alpine) are live
  at `192.168.1.80`, HTTP8123/MQTT1883. Laptop proved authenticated API and MQTT
  QoS1 LAN round trip with original credentials. `The-Octagon.local` resolved to
  the Mini and its frontend returned HTTP200. Router reservation is not verified.
- Idle USB maintenance captured status/fault-log/all eight SD slots, stored only
  a new broker URI in `skulls_net` NVS using private existing credentials, and
  rebooted Jeff. Firmware, Wi-Fi SD settings/assets and logs were not rewritten.
  Jeff connected and rediscovered; both original SD reports read back with the
  same SHA256 before/after. Both laptop containers now remain stopped/exited.
- On the actual Mini-backed Chrome dashboard, selection of Pumpkin flowers was
  acknowledged while idle. Visible Play was accepted; preparing → playing → idle
  showed the correct title and retained speaker connection, no pending fault and
  two reports. This is command/state proof, not an independent audio-quality
  judgment. No Anthony/paired action was attempted.
- Visible Open reports on the Mini fetched both original files successfully.
  Both rendered `<pre>` strings matched the original SD text exactly: 413 and
  5,689 bytes. The original account logged in successfully without credential
  changes. A known expired short-lived test token caused a login notice; private
  helper refresh and browser sign-in passed, and only that notice was dismissed.
- Mini owner restarted only the two new services. An authenticated retained
  non-command canary survived broker restart and was cleared afterward; existing
  five services/Colima were not restarted. Minimum sampled physical free space
  during that test was 2.47GiB. HTTP readiness preceded entity readiness: root
  observed unavailable entities, then fresh raw MQTT every5s and automatic HA
  recovery without device/service/credential intervention. Wait for fresh numeric
  uptime and actual Select availability, not merely HTTP200 or retained online.
- Post-recovery API verified rising Jeff uptime, accepted command result, Idle,
  speaker connected, reportcount2/pendingoff. UI then restored the pre-migration
  **Skit - spears.wav** selection; device acknowledged `selected` and stayed idle.
  No second playback or firmware change was needed. The observed minimum internal
  heap was151 bytes after this run: preserve the distinct firmware resource limit.

### Operating ownership and remaining boundaries

Use the Mini's tracked
`/Users/occam/Developer/openai-bishop/services/home-assistant/compose.yaml` and
its adjacent README plus `docs/runbooks/apps/home-assistant.md`. They are the
owner of image digests, restart policies, persistent paths, protected secrets,
backup/update/rollback commands and current capacity checks. Do not recreate the
service from the old laptop runtime-control script. The existing Mini login
supervisor starts Colima; service restart is demonstrated, full host reboot and
known AC-return behavior are not requalified here. No global VM restart, NAS
change, router/firewall change or public exposure was performed.

On the phone, edit the **existing** HA server's connection URL to
`http://192.168.1.80:8123` (or the verified hostname URL); preserve its migrated
registration. Same account credentials apply. Phone-side reconnection needs Cam's
observation; the laptop cannot operate that app. Verify a router DHCP reservation
before relying on the numeric MQTT endpoint indefinitely. Automatic backup
schedule/off-host restore proof and broader HomeKit/Matter multicast behavior
must be established explicitly rather than inferred from this MQTT migration.

Evidence under private `build/validation/story-007-ha/mac-mini-migration/`:
`transfer-completed-public.json`, `target-startup-validation.json`,
`mqtt-lan-validation.json`, `jeff-endpoint-validation.json`,
`mini-ui-playback-validation.json`, `mini-log-ui-validation.json`,
`mini-post-restart-validation.json`, `mini-final-selection-validation.json`,
`hostname-validation.json`, screenshots and redacted serial captures.
The first restart observer ended before the eventual restart and did not observe
an API outage; it is not counted as restart proof. Owner-side restart/canary
records and subsequent fresh device/UI checks provide that proof.

### Final startup dependency review

Mini owner inspected the actual shared startup and found the existing NAS gate
cannot safely be moved alone: Bookshelf auto-starts via `unless-stopped` with
writable NAS binds, and Colima explicitly mounts `/Volumes/Data`. Starting it
before NAS readiness could mount absent paths incorrectly. No runtime/startup
change was made. Decoupling HA cold startup needs a reviewed shared mount/owner
change and controlled startup test, separate from this live service migration.
Current healthy service operation and two-container recovery are proven; do not
claim NAS-independent cold boot or power-loss recovery.

Mini-side final checkpoint `59a056b` records live data at
`/Users/occam/Library/Application Support/HomeAssistant/`: HA `config/`, broker
`mosquitto/config/` and `mosquitto/data/`. Manual backup/restore procedure exists,
but no recurring HA/MQTT backup schedule, isolated restore or verified Backblaze
coverage is claimed. Protected cold copies remain on both hosts. Final measured
health sample: approximately3.48GiB host free, HA308/768MiB and MQTT3/64MiB,
no OOM, existing household services unchanged. These are measured samples, not a
guarantee of future storage/peak load. The target is left running; laptop services
are stopped, and the browser is left at the working Mini dashboard.
