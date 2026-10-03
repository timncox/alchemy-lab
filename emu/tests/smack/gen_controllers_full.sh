#!/usr/bin/env bash
L=(Clean Retrig Reverse Pitch Speed Gate Buzz Crush Repeat "Rev After" "Tape Stop" "Tape Start" Scratch Env Pan Filter Vowel "Tonal Delay" Freeze Delay Dist Phaser Verb P-Shift "Ring Mod" Comb Scatter)
cat <<'EOF'
# Smack: every controller function (Launchpad Mini MK3, Launch Control XL,
# Haute42), checked against what smack_alchemy.cpp documents. The OLED names
# the effect being punched, so every pad / button is checked by name.
#  Launchpad rows 1-4 = the 27 punch effects (hold); row 8 playhead; top 1
#  CAPTURE, 2 RE-ROLL, 3 LIVE, 4 CLEAR (hold 1 s). XL faders 1-6 = PLAY
#  knobs, top knobs 1-6 = SETUP knobs, upper 1-4 CAPTURE RE-ROLL LIVE CLEAR,
#  lower 1-8 punch effects 1-8. Haute42: every button a punch effect.
#!args --usb launchpad
wait 4500
sing 220
wait 8000                  # longer than LENGTH steps
xl button 1 1              # CAPTURE from the XL
wait 1500
expect led b1 green        # looping
expect lpmoves 8           # the playhead
expect screen Loop
expect lp 2 1 9            # the PUNCH FX knob's effect (Retrig) bright
expect lp 3 1 11           # row 1 dim orange
expect lp 1 2 15           # row 2 dim amber
expect lp 1 3 39           # row 3 dim cyan
expect lp 1 4 55           # row 4 dim magenta
expect lp 4 4 off          # 27 effects: row 4 ends at pad 3
# -- every Launchpad punch pad: green while held, B1 white, the OLED names it
EOF
for fx in $(seq 0 26); do x=$((fx % 8 + 1)); y=$((fx / 8 + 1)); printf 'lp press %d %d\nwait 250\nexpect lp %d %d 21\nexpect led b1 white\nexpect screen %s\nlp release %d %d\nwait 200\n' $x $y $x $y "${L[$fx]}" $x $y; done
cat <<'EOF'
expect led b1 green
# -- XL top knob 4 = PUNCH FX: the bright pad moves; B2 punches that effect
xl knob 1 4 64             # zone 13 of 27: Env
wait 400
expect lp 6 2 13
press b2
wait 300
expect led b1 white
expect screen Env
release b2
wait 300
# -- top 3 LIVE (and XL upper 3)
lp top 3
wait 300
expect lp top 3 37
expect led b1 cyan
expect screen LIVE
expect xl button 1 3 0x3C
lp top 3
wait 300
expect lp top 3 39
xl button 1 3
wait 300
expect led b1 cyan
xl button 1 3
wait 300
expect led b1 green
# -- top 2 / XL upper 2 RE-ROLL: still looping
lp top 2
wait 600
expect led b1 green
expect lpmoves 8
xl button 1 2
wait 600
expect led b1 green
# -- XL lower 1-8 hold punch effects 1-8 (Retrig .. Repeat)
EOF
for c in $(seq 1 8); do printf 'xl button 2 %d press\nwait 250\nexpect xl button 2 %d 0x3C\nexpect led b1 white\nexpect screen %s\nxl button 2 %d release\nwait 200\n' $c $c "${L[$c]}" $c; done
cat <<'EOF'
# -- XL faders = PLAY knobs (level rings count, selector rings move)
xl fader 1 0
wait 300
expect ring 1 < 3
xl fader 1 127
wait 300
expect ring 1 > 10
xl fader 2 0
wait 300
expect ring 2 < 3
xl fader 2 127
wait 300
expect ring 2 > 10
ringsave 3
xl fader 3 127
wait 300
expect ringchanged 3
ringsave 4
xl fader 4 127
wait 300
expect ringchanged 4
xl fader 5 0
wait 300
expect ring 5 < 3
xl fader 5 127
wait 300
expect ring 5 > 10
ringsave 6
xl fader 6 127
wait 300
expect ringchanged 6
xl fader 6 64
xl fader 5 64
# -- XL top knobs = SETUP knobs (shown while B3 is held)
press b3
wait 300
xl knob 1 1 0
wait 300
expect ring 1 < 3
xl knob 1 1 127
wait 300
expect ring 1 > 10
xl knob 1 2 127
wait 300
expect ring 2 > 10
ringsave 3
xl knob 1 3 127
wait 300
expect ringchanged 3
xl knob 1 4 127
wait 300
expect ring 4 > 10
ringsave 5
xl knob 1 5 127
wait 300
expect ringchanged 5
xl knob 1 6 127
wait 300
expect ring 6 > 10
xl knob 1 5 0
xl knob 1 6 50
release b3
wait 600
# -- Haute42: every button holds its effect (the matching Launchpad pad goes
#    green, B2 white, the OLED names it)
EOF
for pf in "x 1" "y 8" "rb 2" "lb 10" "a 15" "b 7" "rt 19" "lt 18" "left 5" "down 12" "right 20" "up 22" "l3 3" "r3 21"; do set -- $pf; fx=$2; x=$((fx % 8 + 1)); y=$((fx / 8 + 1)); printf 'pad %s press\nwait 250\nexpect lp %d %d 21\nexpect led b2 white\nexpect screen %s\npad %s release\nwait 200\n' $1 $x $y "${L[$fx]}" $1; done
cat <<'EOF'
expect finite
# -- top 4 CLEAR (hold 1 s): red while held, then idle and quiet
lp top 4 press
wait 300
expect lp top 4 5
wait 1000
lp top 4 release
wait 600
expect lp top 4 7
expect lpstill 8
expect screen Pattern
silence
wait 600
mark
wait 400
expect rms < 0.01
# -- top 1 CAPTURE from the Launchpad (LENGTH back to its shortest first:
#    at the longest the playhead takes seconds to cross one of 8 pads)
xl fader 3 0
sing 262
wait 8000
lp top 1 press
wait 900
lp top 1 release
wait 1500
expect led b1 green
expect lpmoves 8
EOF
