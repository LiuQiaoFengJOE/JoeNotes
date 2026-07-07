# Embedded Camera Interview Map

## Purpose

Use this reference to keep embedded camera interview preparation complete, balanced, and realistic for small-to-mid experience candidates.

When the user wants the complete stored question bank with answer-style material, also read `full-interview-question-bank.md`.

## Must-Cover Domains

1. Linux fundamentals
   - process vs thread
   - mutex, semaphore, condition variable
   - IPC
   - blocking vs non-blocking IO
   - `select` / `poll` / `epoll`

2. Kernel and driver basics
   - character driver model
   - `platform_driver` / `platform_device`
   - `probe`
   - interrupt top half / bottom half
   - DMA
   - `mmap`

3. Device tree and system trimming
   - `compatible`
   - GPIO / reset / power / interrupt description
   - kernel trimming
   - `rootfs` trimming
   - boot-time analysis

4. Sensor / CMOS bring-up
   - power sequence
   - I2C register programming
   - MIPI vs DVP
   - exposure / gain / frame rate relations
   - bring-up and no-image debugging

5. ISP
   - AE / AWB / AF
   - noise, overexposure, color cast
   - WDR / HDR
   - OSD position in pipeline

6. Codec and mux
   - H.264 / H.265
   - I/P/B, IDR, GOP
   - CBR / VBR
   - SPS / PPS
   - MP4 / TS / MOV

7. Display and UI
   - LCD types
   - preview path
   - tearing, double buffer
   - LVGL `flush_cb`
   - OSD and menu overlay cost

8. Storage and replay
   - SD card throughput
   - FAT32 / exFAT / ext4
   - segmented recording
   - file corruption after power loss
   - playback indexing

9. Streaming and networking
   - RTP / RTCP
   - UDP choice
   - timestamp / sequence number
   - packet loss / jitter / reordering
   - Wireshark-based troubleshooting

10. RTOS awareness
    - RTOS vs Linux
    - scheduling
    - ISR design
    - priority inversion

11. Project deep dives
    - full data path explanation
    - customer bug investigation
    - performance bottleneck analysis
    - mass-production support

12. Troubleshooting
    - no image
    - color issues
    - dropped frames
    - preview / record mismatch
    - WiFi lag
    - LCD black screen
    - reboot / crash

13. Linux media frameworks
    - V4L2
    - media controller
    - `v4l2-ctl`
    - `media-ctl`
    - DRM/KMS
    - `dma-buf`

14. Audio subsystem
    - ALSA
    - I2S / TDM / PDM
    - audio codec chips
    - AEC / AGC / NS
    - A/V sync
    - `G.711` / `AAC` / `Opus`

15. Streaming ecosystem and interoperability
    - RTSP methods
    - ONVIF
    - GB28181
    - WebRTC
    - dual-way audio
    - metadata / events

16. Multimedia frameworks
    - GStreamer pipeline concepts
    - FFmpeg demux / mux / transcode basics
    - hardware acceleration paths
    - segmentation and recording workflows

17. AI / NPU awareness
    - preprocessing / postprocessing
    - stream split for analytics
    - event reporting
    - OSD metadata overlay
    - object detection vs classification vs segmentation
    - YOLO-style detection basics
    - OpenCV preprocessing and visualization
    - NPU model conversion and quantization

18. Reliability and deployment
    - watchdog
    - OTA
    - A/B upgrade
    - secure boot
    - logging and field diagnostics
    - aging / reboot / power-fail testing

19. Performance and memory
    - zero-copy
    - CMA
    - cache coherency
    - bandwidth budgeting
    - queue latency tradeoffs

## 20K Mid-Sized Company Depth Rubric

- Aim for strong breadth, not obscure kernel trivia.
- Weight the output roughly as:
  - `60%` fundamentals
  - `25%` project deep dives
  - `15%` troubleshooting
- Expect interviewers to test:
  - whether the candidate can explain the media pipeline clearly
  - whether the candidate can separate theory from hands-on work
  - whether the candidate can give a usable debugging path

## Output Mix

- Questions-only mode:
  - group by category
  - easy to hard ordering
  - keep each question concise

- Questions plus answer-points mode:
  - give short answer bullets
  - mention one project example whenever the resume supports it

- Mock interview mode:
  - start from self-introduction
  - move from Linux basics to media pipeline
  - finish with troubleshooting and project ownership

## Red Flags

- Do not turn the set into generic C-language trivia.
- Do not ask deep ISP algorithm math unless the resume truly supports it.
- Do not over-focus on one area such as UI while neglecting the camera pipeline.
- Do not ignore audio, interoperability protocols, or reliability topics when the target role is market-facing embedded audio-video work.
- Do not treat beginner users like senior kernel maintainers.
