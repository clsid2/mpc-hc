# Chromecast suite

Tests MPC-HC's Cast sender against the framework's `cast-mock` submodule
(castv2-mock-device): a mock Google Cast receiver providing mDNS discovery,
TLS on 8009, CastV2 protobuf framing, and adversarial failure switches — so
casting behaviour is asserted without Cast hardware, the way the `dvb` suite
asserts tuner behaviour without broadcast hardware.

**Status: scaffold, both halves now exist.** The player side is
[clsid2/mpc-hc#4128](https://github.com/clsid2/mpc-hc/pull/4128)
("Add casting to Chromecast and DLNA devices"), currently open. To run
against it before it merges, build from a branch that has both it and this
directory; once it merges, the suite's tests can land.

What the tests assert, keyed to the PR's actual surfaces:

- **Add-by-address, then the saved list.** The PR's Manage Devices dialog
  supports adding a renderer by address — the deterministic path for tests:
  register the mock by address (no multicast dependency), then assert the
  *Cast to Device* submenu lists it instantly from the saved list, since the
  submenu by design does no discovery.
- **Discovery in the dialog.** Separately, assert the dialog's scan finds the
  mock via mDNS where the network allows, and that discovery
  threads/sockets exist only for the dialog's lifetime (the PR states this;
  the mock's connection log shows it).
- **Session bring-up.** Picking the device stops local playback and opens
  the cast window; CONNECT and default-media-receiver launch are asserted
  from the mock's log, not the player's belief.
- **Serving and playback.** The mock fetches the media from the player's
  local HTTP server; assert byte-range requests arrive and the LOAD carries
  the right content.
- **Transport round trips.** Play/pause/seek/stop/volume from the cast
  window, echoed in the mock's state; seek asserted against the device
  clock, which is also what the PR writes resume positions from.
- **Player independence.** While casting, the player is an ordinary stopped
  player — open and close other files locally and assert the session (and
  the mock's stream) is undisturbed.
- **Failure paths.** The mock's adversarial switches — refused TLS, dropped
  session mid-cast, malformed frames — driving the sender's error handling,
  declared-expectation style like the dvb suite's fault injection.

Out of scope here: the PR's DLNA/UPnP-AV half, which would want an
SSDP/SOAP mock of its own (a natural sibling suite or an extension of this
one).

Run the mock from the submodule (`python ..\..\cast-mock\mock_cast.py`; see
its README for the switches).
