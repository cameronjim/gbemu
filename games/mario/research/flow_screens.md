# flow screens: the audit against smb1

purpose: what the between-play screens do in smb1 (smbdis.asm), what ours did before #18, and what
they do now. every frame count below is smb's: its interval timers (`IntervalTimerControl`
reloads $14) tick once every 21 frames, so a `ScreenTimer` of 7 is 147 frames.

| screen | smb1 | before #18 | now |
|---|---|---|---|
| world / lives card | `DisplayIntermediate` before every level and every respawn, never after a pipe: WORLD x-y, the player sprite, x lives, held `ScreenTimer` 7 intervals (147 frames), silent | missing | left out on purpose: the owner does not want a transition screen on the way into a level. the level loads straight off the map and straight after a death |
| time bonus | `AwardGameTimerPoints` once he is in the doorway: one interval a frame at 50 points, `Sfx_TimerTick` queued on frames whose counter has d2 set, the strip updating as it drains; none in a castle (the manual) | approximate: eight intervals a frame on the clear card, no tick | present: `hud_spend_time_bonus` an interval a frame in the clear itself, the strip repainting through `hud_draw_counters`, the tick; castles skip it |
| death | `PlayerCtrlRoutine` (smbdis 5651): once he is off the bottom, `ldy EventMusicBuffer / bne ExitCtrl` holds the lose-life step until the death jingle (216 frames) has ended; `DelayToAreaEnd` does the same for the clear fanfare | the level reloaded, or the game over card came up, as soon as he fell off the bottom, cutting the jingle | `music_event_busy` holds the death state, and the clear's card, until the event buffer clears |
| 100 coins = 1-up | `CoinTally` rolls over at 100, `NumberofLives` up, `Sfx_ExtraLife` | present, silent | present with the sound |
| fireworks | `GameTimerFireworks`: a contact time ending in 1, 3 or 6 is that many bursts; `InitFireworks` spaces them $20 frames, 48 px left of the castle flag plus `FireworksXPosData`/`YPosData`; `RunFireworks` draws smb's explosion three frames of eight; `FireworksSoundScore` pays 500 with `Sfx_Blast` as each goes out | missing | present, `fireworks_step` in states.c, the bursts drawn with the fireball puff (smb's own `ExplosionTiles` serve both). the highest bursts are held under the hud strip: smb's sky is 240 lines and ours 144 (`kFireworksTopPx`, unmeasured against smbd) |
| game over | `GameOverInter`: the card holds `ScreenTimer` $12, 378 frames; `RunGameOver` lets start end it early; then the title. smb1's continue is a+start on the title | approximate: 120 frames, no start | present: 378 frames or start. continue is picking the file on the file select, which is how smbd keeps progress |

## not in scope, noted

- the play timer ticks every 60 frames here (`kTimerFramesPerTick`) where smb's `GameTimerCtrlTimer`
  reloads $18: one interval every 24 frames. the bible chose 60; the countdown above is right
  either way, but a time bonus is worth less than smb's per second of play.
- smbd condenses its game over screen "like the pause screen" (mariowiki); its layout was not
  measured, so the game over stays text on the card sky.

## tests

`mario_clear_pays_the_time_an_interval_a_frame_with_smb_s_tick`,
`mario_clear_fires_the_fireworks_the_time_s_last_digit_names`, `mario_game_over_card_holds_smb_s_time`,
and `mario_lives_and_game_over` for start ending the hold. the clear route helpers read the contact
time off the strip before the countdown and subtract the bonus and the bursts back out.
