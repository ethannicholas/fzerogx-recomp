# Routes

Scripted input (`GCN_INPUT`, see gcn-recomp `docs/diagnostics.md`) keyed by presented
frame. Start from a blank card (`rm saves/memcard_a.raw`) for the first-run flow.

## Blank card to a Grand Prix race

```sh
rm -f saves/memcard_a.raw
GCN_INPUT="500:A:5,1300:A:5,1340:A:5,1380:A:5,1440:START:5,1500:A:5,1580:LEFT:5,1620:A:5,\
1800:A:5,2000:START:5,2200:A:5,2400:A:5,2600:A:5,2800:A:5,3000:A:5,3200:A:5,3400:A:5,\
3700:A:5,3800:A:6000" ./build/fzerogx --fast
```

| Frame | Press | What it does |
|---|---|---|
| 500 | A | "Create a save file now?" → Yes |
| 1300-1380 | A ×3 | name "AAA" |
| 1440 | Start | cursor to End |
| 1500 | A | "Register this name?" (the cursor starts on No) |
| 1580, 1620 | Left, A | Yes; the save is written |
| 1800 | A | dismiss "Game Data was saved" |
| 2000 | Start | title screen |
| 2200-3700 | A ×9 | Grand Prix, Novice, Ruby Cup, Blue Falcon, course, machine settings, start |
| 3800 | A held | accelerate from the countdown |

The input log of every run (`saves/inputs/<timestamp>/`) replays it exactly with
`--replay=<dir>`, as long as the build has not changed what the game executes.
