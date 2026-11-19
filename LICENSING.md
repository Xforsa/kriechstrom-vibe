# Licensing

KRIECHSTROM for VIBE FM is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, the
full text in [LICENSE](LICENSE)). Copyright (C) 2026 Grigorii Bezborodov.

- GPL-3.0-only: `kriechstrom/` (the firmware, its icon), `tests/`, `tools/build.sh`, `tools/make_icon.py`.
- `core/kriechstrom/` is KRIECHSTROM (a git submodule) under its own licences: GPL-3.0-only, with the parts it lists
  in its `LICENSING.md` under theirs (MIT, MPL-2.0, Apache-2.0, SIL OFL 1.1). The firmware is built from these files.
- Apache-2.0, from the VIBE FM SDK (Copyright 2026 Sven Trogus): `sdk/`, `tools/vibe-build.sh` (the SDK's
  `tools/build.sh` with two lines added, marked in it), `tools/vibe-check.mjs`, `tools/wasmtest.mjs`,
  `tools/media-host.mjs`, and `tools/kx-test.mjs` (`wasmtest.mjs` with the stage's picture added). Their notice is
  `sdk/NOTICE`. The SDK's display fonts are under the SIL OFL 1.1 (`sdk/LICENSE-FONTS`).

"VIBE FM" is a trademark of Sven Trogus; the Apache License does not grant its use. This firmware is built with the
VIBE FM SDK.
