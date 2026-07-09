# Landscape ISP Challenge

This challenge gives you:

- a reference landscape image
- a difficult Bayer raw derived from that landscape
- a deliberately bad ISP starting point

Your task is to tune the ISP values until the result looks as close as possible to the reference scene.

For a step-by-step teaching guide, also read:

- `raw_samples/landscape_tuning_guide.md`

## Files

Reference image:

- `raw_samples/landscape_challenge_reference.png`
- `raw_samples/landscape_challenge_reference_rgb24_640x480.raw`

Challenge Bayer input:

- `raw_samples/landscape_challenge_sensor_rggb8_640x480.raw`

Bad starting output:

- `raw_samples/landscape_challenge_badstart/04_display_rgb24.raw`
- `raw_samples/landscape_challenge_badstart/04_display_preview.png`

## What is already baked into the raw

The Bayer raw already contains intentionally nasty sensor behavior:

- strong color cast
- strong corner shading
- awkward brightness balance

That means during reprocessing, you should mainly tune:

- `--black-level`
- `--wb-r`
- `--wb-g`
- `--wb-b`
- `--lsc-strength`
- `--gamma`
- `--ccm-strength`
- `--saturation`

Do not worry about `--artifact-strength` during the challenge run with `--skip-generate`. At that point the raw has already been created.

## Starting point: intentionally wrong

Run this bad preset:

```bash
./raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/landscape_challenge_sensor_rggb8_640x480.raw --output-dir raw_samples/landscape_challenge_badstart --black-level 16 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

That preset is wrong on purpose.

## Your tuning loop

Copy the command, change only the ISP values, and write into a new folder each time:

```bash
./raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/landscape_challenge_sensor_rggb8_640x480.raw --output-dir raw_samples/landscape_try_01 --black-level 16 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

Then inspect:

```bash
ffplay -f rawvideo -pixel_format rgb24 -video_size 640x480 raw_samples/landscape_try_01/04_display_rgb24.raw
ffplay -f rawvideo -pixel_format gray -video_size 640x480 raw_samples/landscape_try_01/00_sensor_mosaic_rggb8.raw
ffplay -f rawvideo -pixel_format gray -video_size 640x480 raw_samples/landscape_try_01/02_lsc_wb_gray8.raw
```

The program now also generates preview PNGs automatically in the same output folder:

- `00_sensor_preview.png`
- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `03_demosaic_linear_preview.png`
- `04_display_preview.png`
- `05_display_yuv420p_preview.png`

## Suggested tuning order

1. fix `--black-level`
2. fix `--wb-r --wb-g --wb-b`
3. fix `--lsc-strength`
4. adjust `--gamma`

## Regenerate the whole challenge from scratch

Reference landscape:

```bash
./raw_samples/generate_landscape_challenge.exe
```

Rebuild the Bayer challenge input:

```bash
./raw_samples/simple_isp_demo.exe --base-rgb raw_samples/landscape_challenge_reference_rgb24_640x480.raw --generated-bayer raw_samples/landscape_challenge_sensor_rggb8_640x480.raw --output-dir raw_samples/landscape_challenge_source --artifact-strength 1.25 --black-level 68 --wb-r 3.4 --wb-g 1.15 --wb-b 0.82 --lsc-strength 1.55 --gamma 2.2
```

You do not need that last command for normal tuning. It is only there in case you want to rebuild the challenge input.
