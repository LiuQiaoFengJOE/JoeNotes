---
name: embedded-camera-interview-prep
description: Generate tailored interview question banks, answer outlines, mock interviews, and study plans for embedded Linux audio-video roles covering CMOS sensor bring-up, ISP, codec, muxing, V4L2, ALSA, GStreamer, FFmpeg, RTSP, ONVIF, GB28181, WebRTC, display, storage, streaming, device tree, kernel, Linux process/thread basics, object detection, YOLO, OpenCV, AI/NPU deployment awareness, reliability, and RTOS. Use when Codex needs to turn a resume, project list, or target salary band such as 20K into practical interview preparation for embedded video, IPC, action camera, DVR, smart display, or network camera jobs.
---

# Embedded Camera Interview Prep

## Overview

Read the candidate's resume or project summary first. Extract the real platforms, chips, operating systems, interfaces, media pipeline stages, and shipped products before writing any interview content.

Build interview preparation around practical delivery ability instead of pure theory. Favor questions that test chain-of-thought on bring-up, debugging, performance, and mass-production support.

## Workflow

1. Build the candidate profile.
   - Identify years of experience, target role, target salary, company size, and confidence level.
   - Extract concrete technologies such as `Linux`, `RTOS`, `MIPI`, `DVP`, `I2C`, `SPI`, `Sensor`, `ISP`, `VENC`, `MUX`, `RTP`, `RTCP`, `LVGL`, `LCD`, `SD card`, `device tree`, and `kernel trimming`.
   - Distinguish between hands-on ownership and exposure. Do not overstate weak areas.

2. Calibrate difficulty.
   - For a beginner targeting a mid-sized company at about `20K`, keep about `60%` fundamentals, `25%` project deep dives, and `15%` troubleshooting scenarios.
   - Raise difficulty only when the resume clearly shows real driver bring-up, kernel work, or media-pipeline ownership.
   - If the user asks for "all questions," still keep the ordering from easy to hard.

3. Generate the output in layers.
   - Default order: target role summary, categorized interview questions, project deep dives, troubleshooting questions, and optional answer points.
   - If the user asks only for questions, omit answer keys.
   - If the user asks for a beginner-friendly version, keep wording direct and avoid obscure theory-first questions.

4. Anchor the content to real embedded camera work.
   - Tie questions to complete flows such as `Sensor -> VI -> ISP -> VENC -> MUX -> SD card / RTP -> LCD / APP`.
   - Include failure analysis questions for no-image, color shift, dropped frames, file corruption, LCD tearing, WiFi lag, and reboot issues.
   - Prefer interview-style wording over textbook wording.

5. Expand only when helpful.
   - Add adjacent topics such as boot flow, `rootfs`, DMA, zero-copy, cache, and scheduling when they support the target role.
   - For market-oriented expansion, add `V4L2`, `ALSA`, `GStreamer`, `FFmpeg`, `RTSP`, `ONVIF`, `GB28181`, `WebRTC`, audio algorithms, OTA, security, `YOLO`, `OpenCV`, AI/NPU deployment, and factory reliability topics when they fit the target company profile.
   - Label stretch topics as extension material when they are not strongly backed by the resume.

## Output Rules

- Write in Chinese unless the user asks otherwise.
- Group questions by topic and keep headings explicit.
- Keep each question short and natural, like something a real interviewer would ask.
- When the user gives a salary band, reflect the expected depth in the question set.
- When the user gives a resume, create at least one section of project-specific follow-up questions.

## Reference Use

Read [references/interview-map.md](references/interview-map.md) when you need the coverage checklist, depth rubric, or output mix for embedded camera interview prep.
