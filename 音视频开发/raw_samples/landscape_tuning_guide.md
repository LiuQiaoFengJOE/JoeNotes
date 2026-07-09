# Landscape Tuning Guide

This page is the practical way to tune the landscape challenge back toward normal.

## First principle

Do not regenerate the raw while tuning.

Always keep the same Bayer input:

`raw_samples/landscape_challenge_sensor_rggb8_640x480.raw`

That way, every output difference comes only from your ISP parameters.

## What the program is doing

The C pipeline is:

1. black level correction
2. lens shading correction + white balance
3. demosaic
4. display gamma
5. RGB to YUV

The implementation order is in:

- `raw_samples/simple_isp_demo.c`

## Which files matter most

Every run now gives you these previews automatically:

- `00_sensor_preview.png`
- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `03_demosaic_linear_preview.png`
- `04_display_preview.png`
- `05_display_yuv420p_preview.png`

During tuning, focus mainly on:

- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `04_display_preview.png`

## The correct tuning order

Do it in this order every time:

1. black level
2. white balance
3. lens shading correction
4. gamma

If you jump around, you will keep chasing moving targets.

## Step 1: black level

You are fixing the sensor offset first.

Look mainly at:

- dark mountain areas
- dark water
- foreground grass

### If black level is too low

Symptoms:

- the whole image looks washed
- shadows look gray instead of dark
- blacks feel lifted
- contrast feels sleepy

What to do:

- increase `--black-level`

### If black level is too high

Symptoms:

- shadow detail disappears too early
- dark regions turn into flat black masses
- the foreground loses structure

What to do:

- decrease `--black-level`

### Practical target

You want:

- dark areas to feel dark
- but still keep shape and separation

For this challenge, a reasonable search area is:

- `--black-level 50` to `80`

## Step 2: white balance

Only after black level looks roughly sane.

Look at:

- the sky
- the lake reflection
- snow/bright ridge highlights

This challenge starts with a very cyan/cool result if the values are wrong.

In this demo, white balance is the main color knob.
The demo also has a built-in CCM, but you can blend it toward neutral with `--ccm-strength`.
Do not expect gamma to repair a bad tint.
If the image still feels off after WB, you are probably looking at a remaining color-matrix issue, not a brightness issue.
The C demo also exposes `--saturation`, which is the next color knob after WB and CCM.

### If the image is too cyan / too blue-green

Symptoms:

- sky shifts toward cyan
- mountain highlights look mint or turquoise
- water looks unnaturally electric blue

What to do:

- increase `--wb-r`
- usually reduce `--wb-b`
- often reduce `--wb-g` a bit too

### If the image is too red / too warm

Symptoms:

- sky goes purple or dirty magenta
- mountain ridge glows orange-red too much
- bright areas lose neutrality

What to do:

- reduce `--wb-r`
- or slightly increase `--wb-b`

### If the image is too green

Symptoms:

- snow or bright haze looks greenish
- neutral mist becomes yellow-green

What to do:

- reduce `--wb-g`

### Practical target

For this challenge, you should expect:

- `--wb-r` much larger than `1`
- `--wb-b` below `1`
- `--wb-g` around `1`

Good search space:

- `--wb-r 3.0` to `6.0`
- `--wb-g 0.9` to `1.3`
- `--wb-b 0.4` to `0.9`

### After WB, use these two

- `--ccm-strength`
  - lower it if the whole image still feels too tinted or too stylized
  - raise it if the colors are too plain and the image lacks color separation
- `--saturation`
  - lower it if the image feels too loud or neon
  - raise it if the result looks too gray after WB and CCM

### How to think about color

Use the bright, mostly neutral parts as your anchor:

- snow-like ridge highlights
- misty sky areas
- bright water reflections

The goal is not to make every area neutral.
The goal is to make the neutral parts neutral and let the landscape colors stay alive.

### Simple color loop

1. If the whole image is cyan/blue, raise `--wb-r` first.
2. If the whole image is too magenta/red, lower `--wb-r` or raise `--wb-b`.
3. If the whole image is too green/yellow-green, lower `--wb-g` a little.
4. Do not touch `--gamma` to fix tint.
5. If tint is still wrong, lower or raise `--ccm-strength`.
6. If the colors feel too weak or too loud, move `--saturation`.
7. Recheck `04_display_preview.png` after every change.

### Good habit

Change one color value at a time and keep notes.
For example:

- `wb-r: 3.2 -> 3.8`
- `wb-b: 0.85 -> 0.72`

That way you can actually learn what each knob does.

## Step 3: lens shading correction

This is mainly about center-versus-corner balance.

Look at:

- top corners of the sky
- left/right lake edges
- overall brightness from center to edges

### If LSC is too weak

Symptoms:

- corners stay darker than the center
- image still feels like there is a dim vignette
- edge detail feels suppressed

What to do:

- increase `--lsc-strength`

### If LSC is too strong

Symptoms:

- corners become brighter than the center
- edges look lifted unnaturally
- the frame starts to feel like it has a glowing border

What to do:

- decrease `--lsc-strength`

### Practical target

For this challenge, likely useful range:

- `--lsc-strength 1.0` to `2.0`

## Step 4: gamma

Do gamma last.

Gamma is for display feel, not raw correction.

Look at:

- sky gradient smoothness
- overall contrast
- whether midtones feel flat or heavy

### If gamma is too high

Symptoms:

- image looks foggy or milky
- shadows lift too much
- contrast becomes weak

What to do:

- decrease `--gamma`

### If gamma is too low

Symptoms:

- image looks too dark
- midtones feel crushed
- the lake and mountain lose separation

What to do:

- increase `--gamma`

### Practical target

Useful search space:

- `--gamma 1.9` to `2.4`

## Recommended working style

Use one directory per attempt:

```bash
./raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/landscape_challenge_sensor_rggb8_640x480.raw --output-dir raw_samples/landscape_try_01 --black-level 16 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

Then next try:

```bash
./raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/landscape_challenge_sensor_rggb8_640x480.raw --output-dir raw_samples/landscape_try_02 --black-level 60 --wb-r 3.2 --wb-g 1.15 --wb-b 0.85 --lsc-strength 1.10 --gamma 2.30
```

Then compare:

- `landscape_try_01/04_display_preview.png`
- `landscape_try_02/04_display_preview.png`
- `landscape_challenge_reference.png`

## A good teaching rhythm

Use this rhythm:

1. make one big correction pass
2. make one color correction pass
3. make one edge/brightness balance pass
4. make one final contrast pass

Do not change all six numbers wildly every time.

## A sensible first rescue move

Without giving away the final answer, a strong first correction from the bad preset would usually look like:

- raise black level a lot
- raise red gain a lot
- lower blue gain a lot
- lower green gain somewhat
- raise LSC a lot
- bring gamma back down near the low twos

If your output is still obviously cyan after that, your next move is still mostly white balance, not gamma.
