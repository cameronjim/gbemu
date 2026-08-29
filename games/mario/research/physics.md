# SMB1 / SMB Deluxe physics research

target: from-scratch recreation of Super Mario Bros. Deluxe (GBC), which is a faithful port of SMB1 (NES).
primary source used throughout: doppelganger's SMB1 disassembly, `SMBDIS.ASM`, fetched from the canonical
gist and grepped directly (line numbers below refer to that file as of the raw blob fetched 2026-08-28).

- primary source: https://gist.github.com/1wErt3r/4048722 (raw: `SMBDIS.ASM`, 16351 lines)
- this is NES code (60 fps NTSC). GBC SMB Deluxe runs at effectively the same logical 60 fps
  (actual hardware refresh ~59.7275 Hz GB/GBC/GBA, ~61.17 Hz on Super Game Boy — cosmetic, not a physics change).
- confidence markers: **documented** = read directly out of the disassembly or a corroborating
  secondary source with the same number; **inferred** = derived by tracing control flow rather than
  reading an explicit named constant, or corroborated by only one secondary community source;
  **must-measure** = nothing authoritative found, flagged for measurement against our own emulator + a real ROM.

no value below was guessed. everything not sourced from code or a citable secondary is explicitly marked must-measure.

## 0. fixed-point conventions (documented, derived directly from the ADC/shift code, not stated in a single comment)

Two different conventions are used on the two axes — confirmed by reading the actual bit manipulation, not assumed:

- **Horizontal** (`Player_X_Speed`, `Enemy_X_Speed`, `FrictionData` adders): a signed byte where the
  high nibble is the whole-pixel speed (sign-extended) and the low nibble is a 1/16-pixel fraction.
  i.e. **16 subpixels = 1 pixel**. Confirmed at `MoveObjectHorizontally`, smbdis.asm:7541-7556 (four `ASL`s
  move the low nibble to the high byte position, four `LSR`s extract/sign-extend the high nibble) — this
  routine is shared by the player and all enemies via `MoveEnemyHorizontally`/`MovePlayerHorizontally`.
  Secondary corroboration ("16 subpixels = 1 pixel"): https://8bitplumber.github.io/
- **Vertical** (`Player_Y_Speed`): a signed **whole-pixel** value added straight to `Player_Y_Position` each
  frame. A *separate* fractional accumulator, `Player_Y_MoveForce` (1/256-pixel units), is what gravity
  actually adds to every frame; when it overflows/underflows the resulting carry/borrow is what changes
  `Player_Y_Speed` by ±1 whole pixel. Confirmed at `ImposeGravity`, smbdis.asm:7704-7735 and the
  player-specific gravity dispatch at smbdis.asm:6014-6188.
  → acceleration constants below (`JumpMForceData`/`FallMForceData`/`FrictionData`) are all in these
  1/256-pixel-per-frame (horizontal) or 1/256-pixel-per-frame² (vertical) raw units; px/frame(²)
  conversions in physics.json divide by 16 (horizontal) or 256 (vertical, since Y itself is already whole px).
- game timer / interval timers: the engine has two decrement rates. "frame timers" (`Timers` array offset
  0x00-0x14) tick every frame; "interval timers" (offset 0x15-0x23) tick once every **21 frames**
  (`IntervalTimerControl`, reset to `$14`=20, i.e. count 0..20 = 21 frames). Confirmed at smbdis.asm:789-799,
  `Timers = $0780` at smbdis.asm:81. This 21-frame tick is the same one glitch-hunters call "the 21 frame rule"
  (https://8bitplumber.github.io/).

## 1. horizontal movement

All values are Mario's; Luigi uses a different `FrictionData`/`NormalXSpdData` set in 2-player games (not
extracted here — same table shapes, different bytes, `smbdis.asm:6033-6034` shows both).

### 1.1 max speed by context — `MaxLeftXSpdData`/`MaxRightXSpdData`, smbdis.asm:6026-6031 (documented)

| context | hex (R / L) | subpixels/frame | px/frame |
|---|---|---|---|
| running (B held + moving with facing dir, or already fast in air) | `$28` / `$D8` | 40 | 2.5 |
| walking (default ground/air cap) | `$18` / `$E8` | 24 | 1.5 |
| water (swimming) | `$10` / `$F0` | 16 | 1.0 |
| pipe-entrance cutscene (`GameEngineSubroutine`==7 forces this tier, right only found) | `$0C` | 12 | 0.75 |

Cross-check (independent parse of the same disassembly, same numbers): https://simplistic6502.github.io/smb1_tll/smbpedia_movement.html

### 1.2 acceleration/friction — `FrictionData`, smbdis.asm:6033-6034 (documented values; **inferred** index mapping, traced from control flow at smbdis.asm:6139-6188)

| tier | hex | value/256 subpixels/frame² | px/frame² (÷16 again) | when selected |
|---|---|---|---|---|
| run accel | `$E4` (228) | 0.891 | 0.0557 | on ground, B held + facing==moving dir, or `RunningTimer` already active, or airborne with `Player_XSpeedAbsolute` ≥ 25 subpixels (keeps running accel in air) |
| walk accel | `$98` (152) | 0.594 | 0.0371 | default ground/air acceleration otherwise |
| high-speed/skid-adjacent accel | `$D0` (208) | 0.8125 | 0.0508 | reached via the `RunningSpeed` flag or `Player_XSpeedAbsolute` ≥ 33 subpixels while not in the "actively running" case above (`ChkRFast`→`FastXSp`, smbdis.asm:6159-6167) |

Skid/reversal doubling (documented, smbdis.asm:6183-6187): whichever of the three values above is selected
gets shifted left by 1 bit (**doubled**) whenever `PlayerFacingDir != Player_MovingDir` — i.e. skidding
(turning around) is not a separate table, it is 2x whatever the context's normal accel value is. This
directly explains why skid-stops feel snappier than plain deceleration.

Skid-vs-walk animation threshold: `PlayerAnimTmrData`/`ProcSkid`, smbdis.asm:6213-6219 — if
`Player_XSpeedAbsolute` < 11 subpixels (0.6875 px/frame) while reversing input, speed is force-zeroed
instantly instead of skidding (this is the "instant turnaround at low speed" behavior).

### 1.3 air control (documented, smbdis.asm:6139-6167)

Mario **can** accelerate/decelerate horizontally in the air using the same `ImposeFriction`/`FrictionData`
logic as on the ground — there is no separate "air acceleration disabled" flag. The one real restriction:
while airborne, the engine cannot promote you into the "actively running" (`$E4`) accel tier by a fresh
B-press; that tier is only entered on the ground (checks `AreaType`, held B, and `RunningTimer`). In the
air the tier is picked purely from your current `Player_XSpeedAbsolute` magnitude, so you keep whatever
acceleration character you had at takeoff but can't newly earn the run tier by holding B only while airborne.
This matches the commonly cited community rule "if you fall under max walk speed during a jump, you can't
exceed it again until landing" (https://www.speedrun.com/smb1/guides/pbl9d) — that guide's exact wording
was NOT independently refetched (its full source page 403'd on this pass) but the underlying mechanism is
directly confirmed in the disassembly here.

## 2. jump / gravity

### 2.1 speed-tier selection at jump start (documented, `Player_XSpeedAbsolute` check, smbdis.asm:6095-6107)

| horizontal speed at takeoff | tier index |
|---|---|
| 0-8 subpixels (0-0.5 px/frame) | 0 |
| 9-15 subpixels (0.5625-0.9375 px/frame) | 1 |
| 16-24 subpixels (1.0-1.5 px/frame) | 2 |
| 25-27 subpixels (1.5625-1.6875 px/frame) | 3 |
| 28+ subpixels (1.75+ px/frame) | 4 |
| swimming | 5 (6 if in a whirlpool) |

### 2.2 initial jump/swim vertical velocity — `PlayerYSpdData`, smbdis.asm:6020-6021, 6122-6123 (documented)

| tier | hex | px/frame |
|---|---|---|
| 0-2 (slow/walk) | `$FC` | -4 |
| 3-4 (running) | `$FB` | -5 |
| 5 (swim stroke) | `$FE` | -2 |
| 6 (whirlpool) | `$FF` | -1 |

### 2.3 gravity while rising — `JumpMForceData`, smbdis.asm:6014-6015 (documented)

| tier | hex | 1/256 px/frame² | px/frame² |
|---|---|---|---|
| 0-1 | `$20` (32) | 0.125 | 0.00781 |
| 2 | `$1E` (30) | 0.1172 | 0.00732 |
| 3-4 (running) | `$28` (40) | 0.15625 | 0.00977 |
| 5 (swim) | `$0D` (13) | 0.0508 | 0.00317 |
| 6 (whirlpool) | `$04` (4) | 0.0156 | 0.00098 |

### 2.4 gravity while falling — `FallMForceData`, smbdis.asm:6017-6018 (documented)

| tier | hex | 1/256 px/frame² | px/frame² |
|---|---|---|---|
| 0-1 | `$70` (112) | 0.4375 | 0.02734 |
| 2 | `$60` (96) | 0.375 | 0.02344 |
| 3-4 (running) | `$90` (144) | 0.5625 | 0.03516 |
| 5 (swim) | `$0A` (10) | 0.0391 | 0.00244 |
| 6 (whirlpool) | `$09` (9) | 0.0352 | 0.0022 |

### 2.5 how holding the jump button changes gravity (documented, `JumpSwimSub`, smbdis.asm:5922-5936)

This is the core "hold A to jump higher" mechanic and it is exactly this, no more:

1. While `Player_Y_Speed` < 0 (still rising): if A is held on this frame **and** was held on the previous
   frame, keep using the weak "rising" gravity (`JumpMForceData`, table 2.3) for another frame.
2. The instant A is released (and the jump has already risen ≥ `DiffToHaltJump` = 1 pixel from
   `JumpOrigin_Y_Position`, i.e. not counting the very first pixel of the jump), the game immediately dumps
   the strong "falling" gravity (`FallMForceData`, table 2.4) into effect — this is what makes short-hops
   short: releasing the button early cuts the rise off hard.
3. Once `Player_Y_Speed` ≥ 0 (apex reached or already falling) the falling gravity applies regardless of
   button state.

There is no third "held" gravity constant — "held" jump physics = table 2.3 (weak) for as long as you keep
holding + rising; everything else = table 2.4 (strong).

### 2.6 max fall speed (documented, `MovePlayerVertically`, smbdis.asm:7586-7595)

Player max fall speed = **4 px/frame** (`lda #$04` passed as the max-speed argument to the generic
gravity/clamp routine `ImposeGravitySprObj`).

### 2.7 swimming (documented, partial)

- Water-area max horizontal speed: 16 subpixels = 1.0 px/frame (table 1.1).
- Swim-stroke vertical impulse / gravity: tier 5 of tables 2.2-2.4 above (-2 px/frame kick, weak gravity
  both directions ~0.05/0.04 px/frame²).
- Whirlpool: tier 6, even weaker (near-floaty) — separately gated on `Whirlpool_Flag`, smbdis.asm:6113-6115.
- Surface clamp: if swimming and `Player_Y_Position` < `$14` (near the top of the water), `VerticalForce`
  is force-set to `$18` every frame regardless of tier, specifically to stop Mario swimming above the
  water line (smbdis.asm:5939-5943).

## 3. interactions

### 3.1 stomp bounce (documented, smbdis.asm:11439-11513)

| event | `Player_Y_Speed` set to | px/frame |
|---|---|---|
| stomping goomba/green-or-red-koopa/cheep-cheep/hammer-bro/lakitu/bloober (`EnemyStompedPts`) | `$FD` | -3 |
| demoting a paratroopa to a koopa, or re-stomping/kicking a shell (`SBnce`) | `$FC` | -4 |

### 3.2 shell kick (documented, `KickedShellXSpdData`, smbdis.asm:11287-11288)

±48 subpixels/frame = **±3.0 px/frame** horizontal, direction away from the player.

### 3.3 demoted koopa walk speed (documented, `DemotedKoopaXSpdData`, smbdis.asm:11290-11291)

±8 subpixels/frame = ±0.5 px/frame — same magnitude as a normal (non-hard-mode) goomba/koopa walk.

### 3.4 fireball (documented + inferred)

- Horizontal speed: `FireballXSpdData`, smbdis.asm:6324-6325 → `$40`/`$C0` = ±64 subpixels/frame = **±4.0 px/frame** (documented).
- Initial vertical speed when thrown: `$04` = **4 px/frame downward** immediately (no upward arc at launch) — smbdis.asm:6350-6351 (documented).
- Fireball gravity/fall cap: downward force byte `$50` (80/256 ≈ 0.3125 px/frame²), max fall speed 3 px/frame, passed to the shared `ImposeGravity` — smbdis.asm:6359-6364 (documented).
- Bounce height off the ground on each hop: **must-measure** — the bounce-on-landing code lives in the
  background-collision handler that re-sets `Fireball_Y_Speed` on contact, which was not traced in this pass.

### 3.5 star invincibility duration (documented, smbdis.asm:11248-11249, timer mechanics smbdis.asm:789-799 + :81)

`StarInvincibleTimer` ($079F, `Timers` offset 0x1F → interval-timer range) is set to `$23` = 35. Interval
timers tick once per 21 frames → 35 × 21 = 735 frames ÷ 60 fps ≈ **12.25 seconds**.

### 3.6 post-damage invulnerability duration (documented, smbdis.asm:11409-11410)

`InjuryTimer` ($079E, offset 0x1E → also an interval timer) is set to `$08` = 8. 8 × 21 = 168 frames ÷ 60 fps
≈ **2.8 seconds** of the flashing/invulnerable state after taking damage.

### 3.7 flagpole slide/scoring by contact height (documented, `FlagpoleYPosData` + `FlagpoleScoreMods`/`FlagpoleScoreDigits`, smbdis.asm:12150-12151, 6573-6577, 12187-12192)

Five Y-position thresholds are checked from the top of the pole down; the digit-place data resolves to the
classic well-known award table:

| Y-position band (screen row, smaller = higher) | points |
|---|---|
| ≥ `$18` and < `$22` (top) | 5000 |
| `$22`-`$4F` | 2000 |
| `$50`-`$67` | 800 |
| `$68`-`$8F` | 400 |
| ≥ `$90` (bottom) | 100 |

Cross-check (these exact five award amounts are widely and consistently cited across SMB1 community
material — treated as documented since they fall directly out of `FlagpoleScoreMods`=[5,2,8,4,1] paired
with `FlagpoleScoreDigits`=[3,3,4,4,4] placing each digit at the thousands or hundreds column).

## 4. enemy movement

### 4.1 goomba / (green & red) koopa walk speed — `NormalXSpdData`, smbdis.asm:8163-8172 (documented)

±8 subpixels/frame = **±0.5 px/frame** in normal mode, ±12 subpixels/frame = **±0.75 px/frame** in
"primary hard mode" (the harder enemy-speed variant the game switches to later in the game / on later loops).

### 4.2 red koopa edge-turning (documented mechanism, `ChkForRedKoopa`, smbdis.asm:12572-12577)

A red koopa in its normal (undefeated) state is special-cased to skip the background-collision state
transition that other enemies use (the one that lets them walk off a ledge) and instead falls straight
through to the direction-inversion check. This is the mechanism behind "red koopas turn around at ledges,
green koopas walk off them." The generic wall/ledge probe common to all ground enemies is
`DoEnemySideCheck`, smbdis.asm:12592-12611 (checks for a solid block at the enemy's leading foot position,
$00/$14 pixel offsets ahead in the current moving direction).

### 4.3 hammer bro (documented, smbdis.asm:8185-8196, 9204-9282)

- Walk-hesitation timer before first movement: `HBroWalkingTimerData` = `$80`(128)/`$50`(80) frames
  (normal/secondary-hard-mode).
- Hammer throw interval: `HammerThrowTmrData` = `$30`(48)/`$1C`(28) **raw frames** (this timer is a
  per-enemy-slot counter decremented every frame directly, not one of the 21-frame interval timers) →
  0.8 s / 0.467 s between throws.
- Jump vertical speed: `$FA` = **-6 px/frame** (default) or `$FD` = **-3 px/frame** (when hammer bro is in
  the upper half of the screen) — smbdis.asm:9248-9260.
- Jump air-time/animation length: `HammerBroJumpLData` = `$20`(32)/`$37`(55) frames, pseudorandomly and
  hard-mode selected.
- Horizontal "shimmy" while grounded: ±4 subpixels/frame = ±0.25 px/frame, direction flips every 64 frames
  (`FrameCounter` bit 6) — smbdis.asm:9276-9282.

### 4.4 lakitu (documented mechanism only — no single clean speed constant; **inferred/must-measure** for exact numbers)

Lakitu's horizontal speed (`LakituMoveSpeed`) is not a fixed constant: it's adaptively decremented toward
zero via `PlayerLakituDiff` (smbdis.asm:9968-10029) based on the horizontal difference to the player, so it
converges on hovering near the player rather than moving at one set speed. Reappearance after being killed:
`LakituAndSpinyHandler` re-checks every 128 frames (`FrenzyEnemyTimer` = `$80`) and needs
`LakituReappearTimer` ≥ 7 of those checks (≈ 7×128 = 896 frames ≈ 14.9 s) before it will respawn into an
empty enemy slot (smbdis.asm:8275-8306). Exact steady-state hover speed/distance: **must-measure**.

### 4.5 bullet bill (documented, `BulletBillXSpdData`, smbdis.asm:6821-6822)

±24 subpixels/frame = **±1.5 px/frame**, direction set toward the player at fire time
(`PlayerEnemyDiff`, smbdis.asm:6834-6839); cannon fires only if the player is ≥ `$50`-`$28` = 40 px away
horizontally at the moment of firing check (smbdis.asm:6841-6844).

### 4.6 cheep-cheep (documented mechanism; arc shape **inferred/must-measure** — LFSR-driven, not a clean formula)

- Jumping (red) cheep-cheep initial vertical speed: `$FB` = **-5 px/frame** (smbdis.asm:8439-8440), using the
  shared enemy gravity/jump code (same mechanism as table 2, tier not confirmed — treat tier as must-measure).
- Flying (swimming-level) cheep-cheep horizontal speed is picked pseudorandomly from `FlyCCXSpeedData`
  (table not extracted in this pass — **must-measure**) and its vertical bob is driven by a pseudorandom
  difference-adjust against a lookup (`PRandomSubtracter`) rather than a sine function — smbdis.asm:9922-9959.
  Exact bob amplitude/period: **must-measure**.

### 4.7 firebar rotation rate (documented, `FirebarSpinSpdData`/`FirebarSpinDirData`, smbdis.asm:8369-8373)

Five firebar variants (short ×4 orientations + long), speed values `$28`,`$38`,`$28`,`$38`,`$28` and
direction bytes `$00`,`$00`,`$10`,`$10`,`$00`. These feed a `FirebarSpin` routine (smbdis.asm:10639-10658)
that behaves like the same fractional-accumulator/carry mechanism as gravity, but here driving a rotation
*state* byte rather than a position — converting the raw values to an actual angular-degrees/frame figure
was not done in this pass (**must-measure** for the final deg/frame number; the two distinct raw rates,
0x28 vs 0x38, i.e. two speeds — is documented).

### 4.8 platform / lift speeds (found labels only — **must-measure** for exact values)

The disassembly has distinct init/move routines for balance platforms, vertical lifts, large lifts, drop
platforms, and horizontal platforms (`InitBalPlatform`, `InitVertPlatform`, `LargeLiftUp`/`LargeLiftDown`,
`InitHoriPlatform`, `InitDropPlatform`, `PlatLiftUp`/`PlatLiftDown`, smbdis.asm:8106-8121, movement code
around smbdis.asm:9605-9610 and 10332-10347) but their per-type speed constants were not individually
extracted in this research pass — flagged for a follow-up pass or direct measurement.

## 5. timers

### 5.1 level timer tick rate (documented, `RunGameTimer`/`ResGTCtrl`, smbdis.asm:6436-6468)

The displayed timer digit decrements once every **24 raw frames** (`GameTimerCtrlTimer` = `$18`, a
per-frame-range timer, not a 21-frame interval timer) = 24/60 = **0.4 real seconds per in-game timer unit**.
This is the well-known fact that SMB1's "seconds" run 2.5x faster than real time.

### 5.2 power-up emergence from a block (documented, `GrowThePowerUp`, smbdis.asm:7181-7197)

The mushroom/flower/star/1-up rises 1 pixel every 4 frames (gated on `FrameCounter & 3 == 0`) until its
state counter passes `$11` (17), i.e. **16 pixels of rise over 64 frames (≈1.07 s)**, after which it becomes
a mobile object with horizontal speed `$10` = 16 subpixels/frame = 1.0 px/frame.

### 5.3 death / pipe animation frame counts (partially documented, **must-measure** for exact totals)

- On death, `Player_Y_Speed` is set to `$FC` = -4 px/frame (a little upward "pop") and `TimerControl` is
  used as a master pause counter; the death-fall control routine only resumes at `TimerControl` == `$F0`
  (smbdis.asm:5766-5770, 11427-11434). The exact starting value of `TimerControl` at the moment of death
  (which fixes the total death-animation length) was not located in this pass — **must-measure**.
- Pipe-entry/exit and pipe-intro cutscenes reuse the general "pipe intro" horizontal speed cap (table 1.1,
  12 subpixels/frame) but their specific frame-length constants were not extracted — **must-measure**.

## 6. SMB Deluxe (GBC) specific deviations

No GBC-side disassembly was consulted (out of scope of the primary source used here — this whole document
is sourced from the **NES** SMB1 disassembly). What secondary sources say about SMBD's own engine:

- "The game physics are somewhat tighter than in the original version" — cited example: the widest gap in
  World 8-1 (the one requiring a running jump) can be crossed in Deluxe without collecting the coins above
  it, which is not possible in the NES original. Source: https://www.mariowiki.com/Super_Mario_Bros._Deluxe
  (documented as a secondary-source claim; the underlying numeric change to jump distance is **must-measure**
  against SMBD's actual GBC ROM/ourselves).
- Deluxe uses "a different engine" from the NES original with many glitches fixed (e.g. the Minus World
  glitch cannot be performed) — same source.
- A speedrun-community claim (surfaced via search, but the source page returned HTTP 403 on direct fetch so
  this was **not independently re-verified**, treat as low-confidence/inferred): SMBD's real hardware
  frame rate is ~59.7275 Hz on GB/GBC/GBA/GBP/SGB2 and ~61.17 Hz on the original Super Game Boy, and it is
  claimed to be "the only version faster than the original" NES release for speedrunning purposes. Source
  (unverified fetch): https://www.speedrun.com/smb1/guides/v5oa1
- No source found stating SMBD changed any specific named physics constant (acceleration, gravity, jump
  velocity) from the NES values documented above. Given rule 5 (no hardware guessing), **all SMBD-specific
  numeric physics constants should be treated as must-measure against a real SMBD ROM in our own emulator**
  rather than assumed identical to the NES values above, even though the "faithful port" reputation suggests
  they are very close.

## 7. must-measure summary (do not guess these — measure against a real ROM)

- fireball landing-bounce height/velocity
- lakitu steady-state hover speed and follow distance
- flying cheep-cheep horizontal speed table (`FlyCCXSpeedData`) and bob amplitude/period
- jumping cheep-cheep gravity tier (which of the tables in section 2 it actually uses)
- firebar rotation rate converted to degrees/frame (raw tick rate is documented, angle mapping is not)
- platform/lift per-type speeds (balance, vertical, large lift, drop, horizontal)
- exact total death-sequence and pipe-transition frame counts
- any and all SMB **Deluxe**-specific numeric constants (this doc is NES-sourced only)
