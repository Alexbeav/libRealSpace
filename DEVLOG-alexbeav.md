# NeoSC / libRealSpace — Alexbeav fork devlog

Fork of [remileonard/libRealSpace](https://github.com/remileonard/libRealSpace) (Strike Commander engine
reimplementation). Work done July 14–15, 2026 by Alex + Claude. Upstream has issues disabled; all
contributions must go via PRs.

## Local setup

| What | Where |
|---|---|
| Clone (origin = Alexbeav fork, upstream = remileonard) | `F:\Github\libRealSpace` |
| vcpkg | `F:\vcpkg` (set `VCPKG_ROOT=F:\vcpkg` before cmake) |
| Build | `cmake --preset=windows` then `cmake --build ./out/build/windows --config Release -- -m` |
| Tests | `out/build/windows/tests/Release/libRealSpace_tests.exe` (94 tests, all green) |
| Dev deployment (playtest build) | `F:\Games\NeoSC-dev\` — copy fresh `neosc.exe` from `out/build/windows/src/executables/sc/Release/` |
| Stock NeoSC 0.4 release (unpatched reference) | `F:\Games\NeoSC\` |
| GOG DOS original (the oracle for all fidelity questions) | `F:\Games\GOG Games\Strike Commander\` (DOSBox; `SC.dat` = raw 2352-byte/sector CD image, engine reads it directly as `./SC.DAT`) |
| Original DOS saves (reference data) | `...\Strike Commander\cloud_saves\` — `A1.SAV` etc. |

## Branch status (all pushed to Alexbeav fork)

Branches are independent off `upstream/master` except where noted. `dev/combined` merges everything
for the playtest build — never PR this one.

### 1. `fix/savefile-format` — save system (COMPLETE, playtested ✔)
- Air/ground kills were swapped between `Save()`/`Load()`; canonical layout verified two ways:
  in-game KILL BOARD transcription (campaign start, air/ground: PRIMETIME 30/6, PHOENIX 11/27,
  BASELINE 12/18, ZORRO 22/13, TEX 16/17, VIXEN 19/16, HAWK 33/48 — matches PilotsId order) and
  Alex's real DOS save. Fun fact: game data gives Primetime the 36-kill record the manual gives Tex.
- 7th kill-board slot (HAWK, offset 0x1C1) was silently dropped — squadron's top killer vanished.
- Header byte 0x07 is 0x01 in original saves (was written 0xFF); file size 0x255 = 597 bytes (was 593);
  unparsed bytes now survive round-trip via `raw_save_buffer`; 16-bit inventory quantities.
- Save dialog: typed names get `.sav` appended (saves used to be written extensionless and were
  invisible to the `.sav`-filtered load dialog — Alex's "lost" saves were this); empty names refused;
  extension matching case-insensitive; **Enter/KP-Enter confirms** — required removing a second
  `m_keyboard->update()` per frame in `SCFileRequester::checkevents()` that destroyed every
  just-pressed edge (GameEngine::pumpEvents already updates once per frame).
- Tests: `tests/SCStateTests.cpp` (8) + `tests/SCFileRequesterTests.cpp` (2).

### 2. `fix/economy-ledger` — money (COMPLETE, settlement verified ✔, one test pending)
- `CatalogueScene::placeOrder` charged cumulative `weapons_costs * 1000` per order: one $60,000
  AIM-9M debited $60,000,000 and re-billed all previous orders. Mission settlement then deducted
  `weapons_costs` again. Fixed: charge each order once, in dollars; settlement = mission pay
  (gameflow register[1] × 1000) − overhead; `cash` (PREVIOUS CASH) now advances at settlement.
- Verified against Alex's real progression: 5850k → 4350k across a mission = exactly −1500k overhead.
- **Pending test**: buy 1 AIM-9M once the catalogue unlocks (it's story-gated — Stern buys weapons
  early campaign; catalogue = book+calculator on the right of Virgil's desk, manual p.54-55).
  Expect exactly −$60k.

### 3. `fix/flight-controls` — throttle guards (COMPLETE, playtested ✔)
- Reverse thrust (negative throttle) now ground-only; floor is idle (0) airborne; negative thrust
  zeroed on liftoff in both flight models.
- Note: unbounded throttle-up (+1/frame forever, >2000% reachable) was a v0.4-release-only bug,
  already fixed on master before our work.

### 4. `fix/flight-model-units` — flight physics (built on fix/flight-controls; NEEDS PLAYTEST)
The player model (`SCJdynPlane`) plugged metric constants into imperial game data (world is
feet/pounds — the code ships the US standard atmosphere in slugs/ft³, `SCPlane` uses 1116 ft/s sos):
- Air density was 1.225 kg/m³ ≈ **515×** the imperial 0.00238 slugs/ft³ → drag+lift soup: sub-200
  top speeds, lift at walking pace (no stall), floaty everything.
- Speed of sound was 340.3 m/s → transonic drag wall at true ~Mach 0.3. Now 1116/968 ft/s.
- Gravity 9.8 → 32.17 ft/s². HUD knots factor 1.944 (m/s) → 3600/6082 (ft/s).
- `climbspeed` was inverted (`dt/Δy` instead of `Δy/dt`) — corrupted landing report_card.
- `inverse_mass` was `1/(W+fuel)`, treating pounds as mass → all forces ~32× too weak vs gravity
  (masked by soup; after density fix nothing could take off). Now `g/(W+fuel)` (pounds→slugs).
  Result: F-16 TWR ≈ 1.19, rotation ≈ 145–150 kt — real numbers.
- Removed the ten `*3.2f` control-servo fudge factors (tuned for soup-era velocities; caused violent
  rotation rates after the unit fix). Restored SCPlane's original data-driven servo formulas.
- **State when Alex left off**: last playtest said "game VERY fast, unplayable" → answered by the
  fudge-factor removal (last commit, `27caddd`), which is deployed to NeoSC-dev but NOT yet flown.
  First thing next session: fly it. If accel still feels excessive → Mthrust is the afterburner
  rating (23,770 lbf), so 100% throttle = full AB; the fix is mil/AB split (see next steps).

## Next steps (rough priority)

1. **Playtest the flight model** (`NeoSC-dev`): takeoff roll & rotation ~150 kt, top speed (expect
   high-subsonic; >Mach 1.2 level = drag polar needs work), low-speed sink below ~150 kt, control
   feel, landing (report_card should now get sane descent rates), wingman AI takeoff behavior.
2. **Open the PRs to Rémi** once flight model is stable — savefile-format and economy-ledger are
   ready today; flight branches after playtest. First outside contributions to this repo; keep them
   surgical, cite evidence (save-file analysis, kill-board transcription, manual).
3. **Afterburner + original throttle layout**: 100% = mil (~14,600 lbf), AB stages above (6–0 keys),
   `~`=idle, 1–5 = 20/40/60/80/100% mil — per original manual; higher fuel burn on AB. Engine sound
   already switches at 60% (SCStrike.cpp:997-1003).
4. **F-16 replacement charging**: `f16_replacements` is never incremented — jet losses are free.
   Need the replacement price from the original (eject in DOSBox, read the accounts screen).
   Manual p.55: bankruptcy on return-to-base = lose condition — check NeoSC implements that too.
5. **HUD text readability at 1080p+**: bitmap fonts composited into 320×200 framebuffer then
   scaled. Proper fix = render HUD/MFD text at native res (`printText` already takes scale params).
   `super_eagle_2x=false` in `assets/config.ini` helps slightly (already set in NeoSC-dev).
6. **Legacy-save kill migration**: saves written by NeoSC ≤0.4 (detectable: 593 bytes) hold newly
   earned kills in swapped slots. General auto-fix unsafe (depends on load/save history); Alex's
   `alex-003.sav` has 7 kills recorded as ground — patch bytes once Alex says what they really were.
7. **Smaller known issues**: Esc-abort still credits mission kills (SCStrike mission_ended path);
   autopilot has no auto-land (manual says it should); off-runway touchdown skips crash grading
   entirely (bounce, no fireball); AI models (`SCSimplePlane`/`SCVectorPlane`) still have metric
   constants (1.944) and `SCPlane.cpp` still has `GRAVITY = 9.81` + unused metric `AIR_DENSITY`;
   `nocrash` flag semantics look inverted vs its name (SCJdynPlane.cpp:525/538).
8. **Someday**: stall departure modeling (wing_stall exists but is mild), Tactical Operations
   (`neoto.exe` exists; TO TREs are on the CD image), VR (`vrstrike.exe`).

## Verification oracles (how we prove things)

- **Original DOS game in DOSBox** = ground truth for any behavior question (economy amounts, kill
  board, landing grading, autopilot). GOG cloud saves parse with the layout in `SCState.cpp`.
- **Alex's saves** = real-data regression set; the synthetic fixture in `SCStateTests.cpp` encodes
  the campaign-start kill board.
- **Manual PDFs** in the GOG folder (+ `DOCS/` on the CD image) for intended behavior; pilot lore
  in the manual contradicts game data occasionally (Tex/Primetime kill record).
