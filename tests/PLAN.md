# Test framework plan

Where the framework is going, and the smallest set of steps that make the
two suites we have -- `dvb` and `chromecast` -- complete. The broad shape
is stated first so that each small step lands somewhere it will still fit;
nothing in the broad shape is a commitment to build it.

## The shape: a test contract, not a driven UI

The player exposes a small, opt-in test contract, and the framework asserts
through that contract. The human UI is driven only where no contract exists
yet, and every such place is a gap this plan names. Three kinds of surface
make up the contract; each already exists in the tree in some form, which
is the point -- the plan extends what the player already offers rather than adding a
test-only layer beside it.

**Verbs.** Command-line switches that do one thing, write a structured
file, and exit. `/dvbscan <start>-<stop> /dvbscanout <file>` (#4138) is the
model: deterministic, no window driving, no server, no interactive desktop
needed for the result. A verb is the right shape for anything that is a
batch operation -- scan, tune-and-measure, dump state.

**Control.** The `/slave` API in `src/mpc-hc/MpcApi.h`: WM_COPYDATA
commands to a running player (`CMD_OPENFILE`, `CMD_PLAY`, `CMD_SETPOSITION`,
`CMD_SETSUBTITLETRACK`, `CMD_GETAUDIOTRACKS`, `CMD_CLOSEAPP`, ...) and
notifications back (`CMD_STATE`, `CMD_NOWPLAYING`, `CMD_NOTIFYENDOFSTREAM`,
`CMD_CURRENTPOSITION`). It is public, documented, and used by third-party
hosts, so extending it is an ordinary feature change, not test scaffolding.
Control is the right shape for anything live: open this, wait for that,
select a track, observe a session. It needs a host window in the player's
session, which the guest-side harness already has (it runs on the
interactive desktop via the `MpcHarness` scheduled task).

**Observation.** Evidence a test reads after the fact: verb output files,
the `DebugLogMask` log targets written to the app save path (`bda.log`
today; `casting.log` with #4128), the emulator's declared encoding matrix,
and mock verdicts (the cast mock exits non-zero when a check fails). A test
asserts against declared expectations and recorded evidence, never against
what the player believes about itself.

Explicitly *not* part of the contract:

- **The web API** (`/dvb/channels.json` and friends). It serves the human
  web page: capture-mode only, registry pre-stamping, IPv4-only binding,
  read-only. It stays as a feature that can be tested, never as a
  dependency for testing something else.
- **Dialog driving** (WM_COMMAND with control IDs, `BM_CLICK`, `WM_SETTEXT`
  into the tuner-scan dialog). Kept for the one thing it uniquely covers --
  the dialog itself -- and otherwise retired surface by surface as verbs
  and control commands replace it.
- **Golden images.** Frame assertions declare what a frame must contain
  (the emulator's colour streams are self-describing for this reason); no
  pixel-exact references.

The rig model does not change: broker-claimed Hyper-V guests, one claim per
run, the emulator's transport (PowerShell Direct or WinRM) into the guest.
Only suites that render need the interactive desktop; a verb-only suite
could run under a plain remote session.

## Coverage map

What each area would use, and where it stands. "verb", "control" and
"observation" are the three surfaces above.

| Area | Verb | Control | Observation | Status |
| --- | --- | --- | --- | --- |
| DVB scan | `/dvbscan` | -- | scan JSON vs. encoding matrix | **done** (`dvb` suite) |
| DVB tune / decode | `/dvbtune` (proposed) | DVB channel select (proposed `CMD_`) | `bda.log`, frame snapshot | dialog-driven today (`harness.ps1` tuneAndHold) |
| Cast bring-up, serving, failure paths | `/castto <device>` (#4128) | -- | `casting.log`, mock verdict | blocked on a build carrying #4128 |
| Cast transport round trips | -- | transport `CMD_`s routed to the cast session (proposed) | mock's media-command order and clock | after #4128 |
| DLNA / UPnP-AV | `/castto` | as above | needs an SSDP/SOAP mock | not started; sibling suite |
| File playback, tracks, end of stream | `/open /play /sub /dub /close` | `CMD_OPENFILE`, `CMD_GET*TRACKS`, `CMD_SET*TRACK`, `CMD_NOTIFYENDOFSTREAM` | `player.log`, `filtergraph.log` | surfaces exist, no suite |
| Rendered frame | -- | snapshot `CMD_` (proposed) | image file | F5 driving today (`Test-MpcFrame.ps1`) |
| Web API | `/webport` | -- | HTTP responses | feature under test only |
| Crash / hang | every suite | -- | exit code, timeout, `/nocrashreporter` | orchestrator records; not yet asserted per test |

## Minimal now: DVB

The scan path is complete: four standards, headless, asserted from the
written file. Two things make the suite whole, neither a player change.

1. **Provision the matrix streams.** The DVB-T matrix assertion is skipped
   because the tuner carries the colour streams, whose services share
   tsid/onid by design. `New-MatrixStreams` on the DVB-T tuner makes the
   assertion run. The emulator's provisioning scripts default to a single
   guest; a pool of cloned guests has to be provisioned identically, or
   the same suite gives different answers depending on which clone the
   run claims.
2. **Deploy the build under test.** A guest whose deployed player
   predates `/dvbscan` needs `-PlayerBinary` pointed at a fresh build.
   `Build-And-Deploy.ps1` on each guest closes this, and the
   orchestrator's default (`bin\mpc-hc_x64\mpc-hc64.exe`) then holds.

Tune-and-hold and frame assertions stay on the dialog path meanwhile,
available as standalone scripts, not in the suite. The player step that
brings them in is `/dvbtune` (below), and it is deliberately not in the
minimal set.

Acceptance: `Invoke-MpcTests.ps1 -Suite dvb` reports `skipped 0` on a
freshly provisioned guest with no `-PlayerBinary` override.

## Minimal now: streaming

The `chromecast` suite's tests are written against the surfaces #4128
actually provides, and #4128 provides more than the scaffold assumed:
`/castto <name-or-address>` starts a cast from the command line (matching a
saved device first, then discovery for a bounded time), and casting logs to
`casting.log` under the `CASTING` log target. Bring-up, serving and failure
paths therefore need **no player change** and **no window driving** -- each
test is: run the mock with its switches, launch the player with
`<file> /castto <device>`, wait for the player to exit or be closed, read
the mock's verdict and the log.

1. **A build carrying #4128.** Until it merges, build from a branch that
   has both it and this directory, and pass that binary with
   `-PlayerBinary`. The suite's `-Probe` checks the binary for the
   `castto` switch and reports not-ready otherwise, so a build without
   casting skips the suite rather than failing it.
2. **Mock in the guest.** The mock's defaults (loopback address, TTL 1,
   answer-only) are exactly right if it runs where the player runs. That
   needs Python 3.8+ and `openssl` on the guest: an `Install-TestBed.ps1`
   phase, staged the way the WDK media is. Running the mock on the host
   instead would mean advertising a routable address and depending on the
   Hyper-V switch; avoid it.
3. **A media file.** A short clip generated with ffmpeg (already a testbed
   dependency) at provisioning time -- nothing third-party, nothing
   personal.
4. **Tests, in the order they are worth writing.**
   - *Saved device, bring-up.* Stamp a saved-device record for
     `127.0.0.1:8009` (the record format is `CastImplodeFields` in
     `AppSettings.cpp`) and `/castto` it; the mock's verdict asserts auth
     challenge, `CONNECT`, `LAUNCH`, app `CONNECT`, `LOAD`. This is the
     deterministic path and the one the *Cast to Device* submenu uses.
   - *Serving.* Same run; the mock's `Range` check asserts the player's
     media server.
   - *Discovery.* Mock with `--announce`, no saved record; `/castto` by
     name must find it within its search timeout.
   - *Failure paths*, one switch per test, expectation declared the way
     the dvb suite declares fault injection: `--auth-error`,
     `--launch-error`, `--load-failed`, `--invalid-request`,
     `--silent-after N`, `--idle-after N --idle-reason INTERRUPTED`. The
     assertion in every case is that the player ends the session (in
     `casting.log`) and still closes cleanly on request, within a timeout
     -- a sender that hangs is the class of bug the mock exists to catch.
   - *Clean shutdown.* `--expect-idle-at-end`: the sender must leave
     nothing playing when it goes.

Deferred, and why: transport round trips (play/pause/seek/stop from the
sender) need control of the cast session, which is a cast window today; the
right fix is routing the existing transport `CMD_`s to the cast session
while casting (below), not driving the window. DLNA needs its own mock.

Acceptance: `Invoke-MpcTests.ps1 -Suite chromecast -PlayerBinary <#4128
build>` runs bring-up, serving, discovery and at least four failure paths,
all recorded in `summary.json`.

## Player changes, in order

Each is small, upstream-shaped, and unblocks a row of the coverage map.
None is needed for the two minimal sets above.

1. **A `/slave` host in the guest harness** (framework, not player). A
   hidden window in the harness process receives `CMD_CONNECT` and the
   notifications; the harness sends commands instead of WM_COMMAND IDs.
   This alone retires the open/play/close/track driving in `harness.ps1`
   and gives every suite event-driven waits (`CMD_STATE`,
   `CMD_NOTIFYENDOFSTREAM`) instead of polling. It is also what makes a
   `playback` suite (file open, tracks, end of stream) a small job, which
   is what turns this from a DVB-and-cast framework into an MPC-HC one.
2. **Cast-aware transport commands.** While a cast session is active,
   `CMD_PLAY`, `CMD_PAUSE`, `CMD_SETPOSITION`, `CMD_STOP`, `CMD_SETVOLUME`
   act on the session, and `CMD_STATE`/`CMD_CURRENTPOSITION` report it. A
   natural follow-up to #4128; it is what a third-party host would expect
   of the API anyway.
3. **`/dvbtune <channel> /dvbtuneout <file>`.** Tune, hold for a declared
   number of seconds, write signal and decoder state, exit -- the
   `/dvbscan` skeleton reused. Moves tune/decode assertions off the
   dialog.
4. **A snapshot command** (`CMD_SNAPSHOT <path>`, or a switch on
   `/dvbtune`). Retires F5 driving in `Test-MpcFrame.ps1`.
5. **A state dump** (`CMD_GETSTATE` returning JSON, or `/dumpstate
   <file>`). The general answer to "what does the player think right now";
   after this nothing needs the web API for observation.

## Sequence

| Milestone | Delivers | Needs |
| --- | --- | --- |
| M1 DVB complete | `dvb` reports `skipped 0` without `-PlayerBinary` | rig provisioning decision; deploy to the pool |
| M2 Cast minimal | `chromecast` ready and passing its first tests | #4128 build; Python and openssl on the guest; generated clip |
| M3 Control channel | `/slave` host in the harness; a `playback` suite | framework work only |
| M4 Live surfaces | cast transport tests; `/dvbtune` in the `dvb` suite | player changes 2 and 3 |
| M5 Observation | snapshot and state dump; dialog driving retired except for the dialog test itself | player changes 4 and 5 |

M1 and M2 are independent of each other and of everything below them.

## Non-goals

- Pixel-exact golden images.
- Senders that verify Google's device-authentication chain (the mock cannot
  satisfy one, and MPC-HC does not verify).
- Replacing or removing the web API.
- Unit tests of C++ internals: a different layer with different tooling;
  nothing here prevents adding them beside this.
