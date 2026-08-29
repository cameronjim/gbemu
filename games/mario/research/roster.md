# Super Mario Bros. / Super Mario Bros. Deluxe (GBC) — Full Entity Roster

Research notes for a from-scratch SMBD recreation. Every fact below carries its own
source citation and a confidence marker:

- **[verified]** — stated directly on a cited page.
- **[inferred]** — derived from NES/GBC 8x8-tile grid conventions or cross-referenced
  behavior; not a literal quote from a source.
- **[must-verify]** — could not confirm from any accessible source; needs a test-ROM
  or emulator frame-by-frame check before being used as a hardware fact.

Primary sources used:
- mariowiki.com (per-entity articles + the SMBD gallery page)
- themushroomkingdom.net/smb_breakdown.shtml (point-value table)
- spriters-resource.com/game_boy_gbc/smbdeluxe/ (sheet inventory only — the site
  returned HTTP 403 to automated fetches for individual sheet pages; only the top
  category listing was readable through a live browser session, so no per-sprite
  pixel dimensions could be read off it directly. Per instructions, no sheet images
  were downloaded or reproduced.)

---

## 1. Mario / Luigi

| Fact | Value | Confidence | Source |
|---|---|---|---|
| Small Mario size | ~13x16 px (visual sprite), commonly treated as a 16x16 bounding box | [inferred] | web search aggregating NES sprite-dimension discussions (romhacking.net forum topic "Mario's Sprite Dimensions (NES)", not directly fetchable — 403); community consensus figure |
| Super/Fire Mario size | 16x32 px | [inferred] | same as above |
| Luigi | Identical hitbox/size to Mario; palette-swapped (taller-hat/green in later games, but SMB1/SMBD-era Luigi is a green recolor of the Mario sprite) | [verified behavior]/[inferred exact px] | mariowiki.com/Mario (character overview); no pixel dims stated on-page |
| SMBD difference | SMBD adds a distinct 2-player simultaneous/alternating mode and Vs./Boo Race modes; core Mario/Luigi physics otherwise match NES original | [verified] | mariowiki.com Gallery:Super_Mario_Bros._Deluxe overview |

Note: mariowiki's own Mario article does **not** state pixel dimensions anywhere
in the accessible text — this must be verified against an actual disassembly or
emulator inspection before being treated as ground truth.

---

## 2. Goomba

- Behavior: most common enemy, walks back and forth, attacks by walking/charging into Mario ("meant to represent biting"). **[verified]** — mariowiki.com/Goomba
- Defeat methods: stomp, fireball, shell hit, hit from below, Starman contact. **[verified]** — mariowiki.com/Goomba
- Animation: NES original flips between two mirrored sprites for its "walk" rather than using distinct walk frames (memory constraint). **[verified]** — mariowiki.com/Goomba
- Size: 16x16 px (2x2 tiles). **[inferred]** — standard NES/GBC enemy tile grid; not stated on wiki page.
- Points: 100 (stomp/fire/star/below-block). **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Fireballable: yes. Star-killable: yes. Stompable: yes.
- SMBD notes: no roster-level difference found; visual palette modernized for GBC hardware. **[verified general]** — mariowiki.com gallery page

---

## 3. Koopa Troopa (Green / Red)

- Green: walks in straight lines, falls off ledges without turning. **[verified]** — mariowiki.com/Koopa_Troopa
- Red: turns around at ledges ("timid," per the original manual). **[verified]** — mariowiki.com/Koopa_Troopa
- Shell state: stomping forces it into its shell; shell can be kicked to slide and hit other enemies/blocks; a Koopa can also pop back out and re-emerge to retrieve its shell. **[verified]** — mariowiki.com/Koopa_Troopa
- Defeat: stomp, shell hit, Starman contact, fireball (only converts to shell first, doesn't outright remove in SMB1 rules — needs confirmation). **[must-verify exact fireball interaction in SMB1 specifically]**
- Size: ~16x24 px upright / 16x16 px shelled. **[must-verify]** — a web-search summary claimed "16x23" for SMBD sprites, but this could not be confirmed against the actual spriters-resource page (403 blocked); treat as unverified until measured directly.
- Points: 100 (stomp/below-block), 200 (fire/star). **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes. Fireballable: yes (removes shell/defeats). Star-killable: yes.

---

## 4. Koopa Paratroopa (Green / Red)

- Winged Koopa Troopa variant. **[verified]** — mariowiki.com/Koopa_Paratroopa
- Red: flies up and down in a set vertical path. **[verified]**
- Green: bounces toward the player or flies back and forth horizontally. **[verified]**
- Stomping removes the wings, converting it into a regular (green or red) Koopa Troopa; a second stomp then shells it normally. **[verified]** — mariowiki.com/Koopa_Paratroopa
- Size: same footprint as Koopa Troopa plus wing sprite overhang. **[inferred]**
- Points: 400 (stomp — first hit while winged), 200 (fire/star). **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes (de-wings, doesn't kill outright). Fireballable: yes. Star-killable: yes.

---

## 5. Buzzy Beetle

- Quadrupedal Koopa variant, hard dark-blue shell, small (comparable in stature to a Koopa Troopa). **[verified]** — mariowiki.com/Buzzy_Beetle
- Walks toward the player and off ledges without turning, like green Koopa Troopa. **[verified]**
- **Fireproof**: immune to fireballs due to its hard shell. **[verified]** — mariowiki.com/Buzzy_Beetle
- Shell can be kicked like a Koopa Shell. **[verified]**
- Wall/ceiling climbing and drop-attacks are documented on the wiki but are attributed broadly across the franchise, not confirmed specifically for the 1985 NES SMB1/SMBD moveset — treat that particular behavior as **[must-verify for SMB1/SMBD specifically]**, likely a later-game (3D World-era) addition.
- Points: 100 (stomp/below-block only — no fireball value since it's fireproof). **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes. Fireballable: no. Star-killable: yes.

---

## 6. Spiny + Spiny Egg

- Spiny: small quadrupedal, spike-covered shell. Cannot be safely stomped — jumping on one hurts Mario. **[verified]** — mariowiki.com/Spiny
- Defeated only by fireball, Starman, or (in some games) POW block — not by jumping. **[verified]**
- Spiny Egg: thrown by Lakitu; in the **original SMB1, Lakitu drops the egg straight down** rather than arcing it (arcs are a later-game feature). **[verified]** — mariowiki.com/Spiny_Egg
- Egg quantity thrown scales with Mario's power state: 1 egg if Small, 2 if Super/Fire. **[verified]** — mariowiki.com/Spiny_Egg
- Egg hatches into a walking Spiny on ground contact. **[verified]**
- Points: Spiny Egg listed as "200 PTS" in the original instruction manual; Spiny itself 200 (fire), Spiny Egg 100 (below-block). **[verified]** — mariowiki.com/Spiny_Egg + themushroomkingdom.net/smb_breakdown.shtml
- Stompable: NO (damages Mario instead). Fireballable: yes. Star-killable: yes.

---

## 7. Lakitu

- Rides a small white cloud; ring-patterned shell (distinct from other Koopas' hex pattern). **[verified]** — mariowiki.com/Lakitu
- Hides in cloud ~1 second before throwing a Spiny Egg. **[verified]**
- Retreats offscreen automatically as the player approaches the flagpole. **[verified]**
- Respawns after several seconds if defeated (doesn't vanish permanently within a level in SMB1). **[verified]**
- Points: 800 (stomp), 200 (fire). **[verified]** — mariowiki.com/Lakitu, cross-checked themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes. Fireballable: yes. Star-killable: yes (inferred, not explicitly separately stated).
- Size: rider + cloud sprite, wider than standard Koopa due to the cloud base. **[inferred]**

---

## 8. Hammer Bro

- Taller than a standard Koopa Troopa; yellow body, green shell/helmet. **[verified]** — mariowiki.com/Hammer_Bro
- Hops continuously while idle and fighting; jumps nearly as high as Mario. **[verified]**
- Throws hammers at a low/far or high/close angle depending on distance to the player. **[verified]**
- SMB1/SMBD note: depicted **without shoes** (shoes were added starting with a later spinoff title). **[verified]** — mariowiki.com/Hammer_Bro
- Points: 1000 regardless of defeat method (stomp/shell/fire/star) in the original SMB1 — SMB3's "stomp-only for full value" rule is a *later game* change, not SMB1/SMBD. **[verified]** — mariowiki.com/Hammer_Bro, themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes. Fireballable: yes. Star-killable: yes.

---

## 9. Bullet Bill + Bill Blaster (launcher)

- Bullet Bill: flies in a straight line, ignoring gravity, after being fired. **[verified]** — mariowiki.com/Bullet_Bill
- Stompable in SMB1 (later titles like SM64 make some variants unstompable — not relevant to SMB1/SMBD). **[verified]**
- Bill Blaster: cannon/turret that fires Bullet Bills; **will not fire while the player stands directly above it or immediately adjacent to it** (a de-spawn/safety rule). **[verified]** — mariowiki.com/Bill_Blaster
- Bill Blaster itself is indestructible in SMB1. **[verified]** — mariowiki.com/Bill_Blaster
- Points: Bullet Bill stomp = 200. **[verified]** — mariowiki.com/Bullet_Bill, themushroomkingdom.net/smb_breakdown.shtml
- Stompable (Bullet Bill): yes. Fireballable: yes (inferred/standard). Star-killable: yes.
- Stompable (Bill Blaster): no — indestructible obstacle.

---

## 10. Cheep-Cheep (Red / Grey)

- Grey: swims slowly through water. **[verified]** — mariowiki.com/Cheep_Cheep
- Red: moves faster; in lava-level contexts, leaps up from the bottom of the screen. **[verified]**
- Points: 200 (fire/stomp) for the land/leaping variety per themushroomkingdom's table. **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Stompable: yes (leaping ones on land/arcs; underwater stomping is a later-game addition per mariowiki, so **[must-verify]** whether underwater Cheep-Cheep is stompable in SMB1 specifically — treat as not stompable underwater unless proven otherwise). Fireballable: yes. Star-killable: yes.
- Size: small fish sprite, roughly Goomba-scale. **[inferred]**

---

## 11. Blooper

- Small squid-like enemy, cannot touch the ground, follows Mario in a zig-zag swimming pattern. **[verified]** — mariowiki.com/Blooper
- SMBD-specific palette: original NES/All-Stars versions used a tan/brown (Goomba-derived) or pink palette; **SMBD recolors Blooper an off-white color.** **[verified]** — mariowiki.com/Blooper
- Points: 200 (fire) per themushroomkingdom; a separate 1000-point value cited on mariowiki is specifically for **The Lost Levels**, not SMB1/SMBD — do not conflate. **[verified]** — mariowiki.com/Blooper, themushroomkingdom.net/smb_breakdown.shtml
- Stompable: only while airborne/jumping (per general series behavior); not confirmed for SMB1 underwater state — **[must-verify]**. Fireballable: yes. Star-killable: yes.

---

## 12. Podoboo (Lava Bubble)

- Jumps out of lava in simple vertical/arc patterns to attack Mario. **[verified]** — mariowiki.com/Podoboo (fetched as "Lava Bubble," the modern name for the same enemy)
- **Completely invincible in the original Super Mario Bros.** — cannot be defeated by any means in SMB1 (later games add Starman/ice/hammer kill options). **[verified]** — mariowiki.com/Podoboo
- Stompable: no. Fireballable: no. Star-killable: no (in SMB1 specifically).
- Points: none (indestructible obstacle). **[inferred from invincibility]**
- Size: single large fireball sprite, larger than the multi-ball Fire Bar segments. **[inferred]**

---

## 13. Fire Bar

- Standard length: about 6 fireball segments; the World 5-4 bar in the original game is unusually long at 12 segments. **[verified]** — mariowiki.com/Fire_Bar
- Rotates clockwise or counterclockwise around a fixed pivot fireball. **[verified]**
- Indestructible obstacle — damages Mario on contact, cannot be defeated. **[verified]**
- SMBD: no roster differences found beyond palette. **[inferred — not explicitly contradicted by any source]**
- Stompable/fireballable/star-killable: no/no/no (Mario is simply invincible to it while starred, but the Fire Bar itself isn't "killed").

---

## 14. Piranha Plant

- Green head with yellow spots on an orange stem, emerges from vertical pipes. **[verified]** — mariowiki.com/Piranha_Plant
- **Will not emerge if the player is standing on or directly adjacent to its pipe** — a specific, deliberate safety rule. **[verified]**
- Only defeated by fireball or Starman contact — **cannot be stomped**. **[verified]**
- Technical note: an invisible/submerged Piranha Plant exists in World 8-4's underwater section (behind water tiles); Piranha Plants also despawn during Warp Zone message display and are capped by the on-screen enemy limit. **[verified]** — mariowiki.com/Piranha_Plant
- Points: not explicitly stated on the fetched page; themushroomkingdom's table lists 200 (fire/star) for Piranha Plant. **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Stompable: no. Fireballable: yes. Star-killable: yes.

---

## 15. Bowser (+ fire breath)

- Final boss of each castle level (including the 7 "fake Bowser" encounters before the real one in World 8-4). **[verified]** — mariowiki.com/Bowser
- Throws hammers in later castle encounters. **[verified]**
- Defeated either by touching the Axe (drops the bridge, Bowser falls into lava) or by hitting him with 5 fireballs. **[verified — axe mechanic corroborated separately by mariowiki.com/Axe]**
- If already defeated via fireballs before reaching the axe, the bridge-collapse *animation* doesn't replay, though the bridge is still mechanically destroyed. **[verified]** — mariowiki.com/Axe
- Points: 5000 (fireball-defeat only, per the original point table). **[verified]** — themushroomkingdom.net/smb_breakdown.shtml
- Original NES sprite had yellow hair/black eyebrows on a grayish-blue body — different from the now-standard red-haired design. **[verified]** — mariowiki.com/Bowser
- Stompable: no. Fireballable: yes (5 hits). Star-killable: unclear/likely walks through him without defeating him — **[must-verify]**.

---

## 16. Boo (SMBD "You vs. Boo" race mode only)

- **Boo does not appear as an in-level enemy in SMB1 or SMBD's main game** — it is exclusive to the **You vs. Boo** race bonus mode. **[verified]** — mariowiki.com/Boo (SMBD section)
- Does not damage the player on contact — it's a race opponent, not a hazard. **[verified]**
- Difficulty ramps by color as the player wins: white Boo → faster green Boo → faster red Boo → Black Boo (outlined yellow). **[verified]**
- The Black Boo specifically replays/mimics the player's own best recorded time. **[verified]**
- No point value or stomp/fireball/star interaction documented — it's a non-combat racer. Flags set to null/not-applicable.

---

## 17. Springboard / Jumping Board

- Manual mechanic: jumping onto it bounces Mario up and down; pressing the jump button exactly when it's at the top of its bounce launches Mario "superhigh." **[verified]** — mariowiki.com/Jumping_Board (quoting the SMB1 instruction booklet)
- Appears in SMB1 Worlds 2-1, 3-1, 5-2, 6-3, 7-1, 8-2. **[verified]**
- SMBD adds a smaller "Trampoline Floor" variant used only in VS. Game and You vs. Boo modes. **[verified]** — mariowiki.com/Jumping_Board

---

## 18. Platforms (static / moving / balance / falling)

All from mariowiki's Lift article, describing SMB1 mechanics: **[verified]** — mariowiki.com/Lift

- **Static wooden lifts** — visual style derived from the Donkey Kong 100m platforms.
- **Vertical lifts** — continuously move up or down, with new platforms dispensed from the opposite screen edge; first appears World 1-2.
- **Stationary vertical lifts** — stay on screen, oscillate up/down; introduced World 1-3.
- **Horizontal lifts** — move laterally; also World 1-3.
- **Elevator lifts** — narrower paired platforms, scroll off screen like vertical lifts; introduced World 2-4.
- **Balance (see-saw) lifts** — two platforms on a pulley; standing on one lowers it and raises the other; overloading breaks both. Introduced World 3-3.
- **Falling ("flimsy") lifts** — sink downward under Mario's weight; also World 3-3.

No pixel dimensions are stated; standard NES/GBC tile-based platform width is typically a multiple of 16px. **[inferred]**

---

## 19. Flagpole + Flag

- Height-based scoring at level end: 100 / 400 / 800 / 2000 / 5000 points depending on contact height. **[verified]** — mariowiki.com/Goal_Pole, cross-checked themushroomkingdom.net/smb_breakdown.shtml
- Fireworks bonus tied to specific timer end-digits (1/3/6) in the original game — themushroomkingdom quantifies this as 500 points per firework in SMB1 (later NSMB-style "double-digit" fireworks and 4000-pt values are a *different, later* game's rule — do not conflate with SMB1/SMBD). **[verified, with explicit game-version separation noted]**
- **SMBD-specific fix**: the original NES glitch that let players vault clean over the flagpole is patched — SMBD "makes it impossible" to jump over it. **[verified]** — mariowiki.com/Goal_Pole

---

## 20. Axe (+ Bridge)

- Found at the end of every castle level; touching it cuts the bridge supports, dropping Bowser (or the fake) into the lava. **[verified]** — mariowiki.com/Axe
- Works even if Bowser was already defeated by fireballs first (see Bowser section above for the animation-skip nuance). **[verified]**

---

## 21. Coin

- Worth 200 points each. **[verified]** — mariowiki.com/Coin
- Collecting 100 coins grants an extra life — confirmed explicitly for SMB1, The Lost Levels, **and SMB Deluxe** (i.e., unchanged across all three). **[verified]** — mariowiki.com/Coin
- No pixel dimensions or animation-frame count found on the fetched page — **[must-verify]**.

---

## 22. Red Coin (SMBD Challenge Mode)

- SMBD-exclusive addition, not present in the 1985 original. **[verified]** — mariowiki.com/Medal_(Super_Mario_Bros._Deluxe); StrategyWiki SMBD/Challenge guide
- Exactly **5 Red Coins hidden per level** in Challenge Mode; collecting all 5 earns the **Red Coin Medal**. **[verified]** — mariowiki.com/Medal_(Super_Mario_Bros._Deluxe)
- This is a clear **SMBD roster addition** vs. the NES original.

---

## 23. Yoshi Egg (SMBD Challenge Mode)

- SMBD-exclusive addition; one hidden Yoshi Egg per Challenge Mode level, often stashed in a hidden block. **[verified]** — StrategyWiki Super_Mario_Bros._Deluxe/Challenge; mariowiki.com/Medal_(Super_Mario_Bros._Deluxe)
- Collecting it awards the **Egg Medal**; a fan strategy guide cites **2000 points** for grabbing it, though this specific number came from a secondary guide (GameFAQs-adjacent), not mariowiki directly — **[must-verify against a primary source/manual]**.
- World 1-1's egg is documented as sitting in a hidden block within the first staircase, with a target score to beat of 32000. **[verified]** — StrategyWiki

---

## 24. Medals (SMBD Challenge Mode)

- **Red Coin Medal** — all 5 Red Coins collected. **[verified]**
- **Egg Medal** — Yoshi Egg collected. **[verified]**
- **High Score Medal** — score exceeds the level's preset Challenge Score. **[verified]**
- All three are SMBD-only additions layered onto Challenge Mode (a re-play of Original 1985 Mode levels), unlocked only after first clearing that level in the main mode. **[verified]** — mariowiki.com/Medal_(Super_Mario_Bros._Deluxe)

---

## 25. Powerups: Super Mushroom

- NES original color: yellow cap with red spots (the now-standard red-cap/white-spot look came later, from Super Mario World onward). **[verified]** — mariowiki.com/Super_Mushroom
- Moves at Mario's walking speed once released from a block, reversing on wall contact. **[verified]**
- Effect: Small → Super Mario (can take one hit without dying, can break bricks). **[verified]**
- No SMB1-specific point value found (the "1000 pts" figure on the wiki page is explicitly attributed to **New Super Mario Bros.**, a later game — do not use for SMB1/SMBD).

---

## 26. Powerups: Fire Flower

- Original 1985 design: white petals, orange center, green stem/leaves (redesigned multiple times in later games — SMB1/SMBD keep the original look). **[verified]** — mariowiki.com/Fire_Flower
- Stationary item (does not slide/move like the mushroom). **[verified]**
- Effect: grants Fire Mario, throw fireballs via B button, max 2 fireballs on screen at once. **[verified]**
- Getting hit while Fire reverts straight to Small Mario in SMB1 (the "revert to Super first" cushioning rule is an SMB3-only change — do not conflate). **[verified, with version separation noted]**
- No SMB1 point value found on the fetched page — **[must-verify]**.

---

## 27. Powerups: Super Star (Starman)

- Five-pointed star with a face, yellow. **[verified]** — mariowiki.com/Super_Star
- Grants temporary full invincibility to enemy contact (does not protect from pits/lava/time-out). **[verified]**
- Enemy-chain scoring while starred (200-400-800-1000-2000-4000-8000-1UP-1UP-1UP) is cited on the wiki as the pattern for "most modern titles" — **[must-verify this exact chain applies unchanged in 1985 SMB1/SMBD]** rather than being a later-game refinement.
- Invincibility duration figures found (6-10s) are all from **Mario Kart**, not the platformer — **not applicable to SMB1/SMBD; do not reuse.**

---

## 28. Powerups: 1-Up Mushroom

- Green cap with white spots is the modern standard design (from Super Mario World onward); **earlier titles including the SMB1/SMBD era used a yellow-with-green-spots (overworld) or brown-with-teal-spots (underground) palette** instead — an important palette distinction for period-accurate art. **[verified]** — mariowiki.com/1-Up_Mushroom
- Effect: grants an extra life. **[verified]**
- SMBD is explicitly noted as using "the contemporary [SMW-era] visual design" rather than the 1985 palette — **worth flagging as a deliberate SMBD art update vs. the NES original.** **[verified]** — mariowiki.com/1-Up_Mushroom

---

## 29. Powerup: Poison Mushroom — NOT in SMB1/SMBD

- **Poison Mushroom first appeared in Super Mario Bros.: The Lost Levels (1986) and does not appear in the original Super Mario Bros. or Super Mario Bros. Deluxe.** **[verified]** — mariowiki.com/Poison_Mushroom
- Documented here specifically as a **roster omission** — the milestone brief asked to flag this, and it should be flagged as out-of-scope/absent for a from-scratch SMB1/SMBD recreation unless a Lost Levels mode is later added.

---

## 30. Vine / Beanstalk

- Emerges and rapidly grows from a hit block, stopping at the first solid ceiling it reaches. **[verified]** — mariowiki.com/Vine
- Used in SMB1 to reach secret areas like Coin Heaven and Warp Zones. **[verified]**
- Growing downward via ground-pound is an **NSMB-era addition, not SMB1/SMBD** — flagged explicitly to avoid conflation. **[verified, with version separation noted]**
- No SMBD-specific mechanical differences found. **[verified — page groups SMB1/SMBD together]**

---

## 31. Warp Pipe

- SMB1 pipes come in 4 colors: green (most common/standard), silver (castle/snow levels), orange, and purple (rarest, in warp zones). **[verified]** — mariowiki.com/Warp_Pipe
- Some pipes contain a Piranha Plant (see Piranha Plant section for the "won't emerge near the player" rule). **[verified]**
- Certain Lost Levels warp zones use pipes to send the player back to a previously visited world — a Lost-Levels-specific mechanic, noted for completeness but not part of SMB1/SMBD's own roster. **[verified, version-scoped]**
- No tile-width/height figures were stated on the fetched page — **[must-verify]**, though standard pipe openings are conventionally 2 tiles (16px) wide.

---

## 32. Rescue Scenes: Toad & Princess Peach

- Worlds 1-7 castles: a Toad greets Mario with **"Thank you Mario! But our princess is in another castle!"** after each fake-Bowser defeat. **[verified]** — web search cross-referencing mariowiki/Know Your Meme/Wikipedia's "Our princess is in another castle!" article
- World 8-4 (real Bowser): Princess Toadstool (Peach) says **"Thank you Mario! Your quest is over. We present you a new quest."** **[verified]** — same source cluster
- These are scripted, non-combat cutscene entities/text triggers rather than gameplay entities with stats — recorded here as interactive objects per the milestone's request for rescue scenes.

---

## Score / Points Reference Table

Compiled from **themushroomkingdom.net/smb_breakdown.shtml** (primary, all entries **[verified]** unless noted) and cross-checked against mariowiki per-entity pages where overlapping:

| Source of points | Value |
|---|---|
| Goomba (stomp/fire/star/below) | 100 |
| Koopa Troopa (stomp/below) | 100 |
| Koopa Troopa (fire/star) | 200 |
| Koopa Paratroopa (stomp) | 400 |
| Koopa Paratroopa (fire/star) | 200 |
| Buzzy Beetle (stomp/below) | 100 |
| Spiny (fire) | 200 |
| Spiny Egg (fire) | 200 |
| Spiny Egg (below) | 100 |
| Lakitu (stomp) | 800 |
| Lakitu (fire) | 200 |
| Hammer Bro (stomp/fire/star/below) | 1000 |
| Bullet Bill (stomp/star) | 200 |
| Cheep-Cheep, land (fire/stomp) | 200 |
| Blooper (fire) | 200 |
| Piranha Plant (fire/star) | 200 |
| Bowser (fire, 5 hits) | 5000 |
| Coin | 200 |
| Magic Mushroom / Fire Flower / Starman (pickup) | 1000 |
| Brick block destroyed | 50 |
| Time bonus | 50 per second remaining |
| Fireworks bonus (SMB1, timer ends 1/3/6) | 500 each |
| Flagpole: bottom | 100 |
| Flagpole: low-mid | 400 |
| Flagpole: mid | 800 |
| Flagpole: near top | 2000 |
| Flagpole: top | 5000 |
| Stomp chain (consecutive) | 100, 200, 400, 500, 800, 1000, 2000, 4000, 5000, 8000, then 1-Up each |
| Shell-kick chain (consecutive) | 500, 800, 1000, 2000, 4000, 5000, 8000, then 1-Up each |
| 100 coins | 1 extra life |

Note: the two "chain" sequence lists above appear on **both** themushroomkingdom and
mariowiki.com/Point with slightly different starting steps quoted (mariowiki's stomp
chain starts "100, 200, 400, 500, 800, 1000, 2000, 4000, 5000, 8000" and matches
themushroomkingdom's version exactly, so this is treated as **[verified]** via
agreement between two independent sources).

---

## SMBD vs. NES roster differences (summary)

**Additions in SMBD:**
- Red Coins (5 per level, Challenge Mode)
- Yoshi Egg (1 per level, Challenge Mode)
- Red Coin Medal / Egg Medal / High Score Medal
- Boo (as a race-mode opponent only — "You vs. Boo"), with green/red/Black Boo difficulty tiers
- VS. Game and You vs. Boo exclusive "Trampoline Floor" (smaller springboard variant)
- Flagpole-vault glitch patched out

**Omissions / not carried over:**
- Poison Mushroom never appears (it's a Lost Levels-only powerup, and SMBD does not include Lost Levels content in its main roster)

**Unchanged (confirmed via explicit "grouped with SMB1" wording on wiki pages):**
- Coin values and the 100-coin extra-life rule
- Vine/beanstalk mechanics
- Core enemy roster and point values

**Palette/art-only changes noted:**
- Blooper recolored off-white in SMBD (vs. tan/brown NES, pink in All-Stars)
- 1-Up Mushroom uses the modern (SMW-era) green-cap design in SMBD instead of the 1985 yellow/brown palette

---

## Could not size or source (flagged for test-ROM/emulator verification)

- Exact pixel bounding boxes for nearly every enemy sprite (Koopa Troopa/Paratroopa,
  Buzzy Beetle, Lakitu, Hammer Bro, Cheep-Cheep, Blooper, Podoboo, Piranha Plant,
  Bowser) — mariowiki's prose articles do not state pixel dimensions, and
  spriters-resource.com blocked automated fetches of its individual sheet pages
  (HTTP 403); only the top-level category listing (32 total assets across
  Playable Characters/Enemies & Bosses/Backgrounds/Albums/Mystery
  Room/Miscellaneous) was viewable through a live browser session.
- Animation frame counts for nearly all entities — none of the fetched mariowiki
  pages stated exact frame counts (Goomba's 2-sprite-flip "walk" is the one
  documented exception).
- SMBD-specific GBC palette values (actual RGB/GB color indices) for any entity —
  no source gave numeric palette data; only qualitative color-shift descriptions
  (e.g. "off-white Blooper," "softer colors on VS. Boo blocks" per a spriters-resource
  user comment, itself anecdotal/unverified) were found.
- Star-killability of Bowser specifically (walking through him while starred vs.
  defeating him) — not documented in any fetched source.
- Underwater-stomp rules for Cheep-Cheep and Blooper as they specifically apply to
  SMB1 (vs. later games where underwater stomping was added).
- Exact Yoshi Egg point value (2000 pts) — only sourced from a secondary strategy
  guide summary, not a primary wiki/manual citation.
