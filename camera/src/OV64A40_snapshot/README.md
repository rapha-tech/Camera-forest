# OV64A40 snapshot

Takes one photograph with a Omnivision OV64A40 on
an ESP32-P4 and sends it down the console UART as a JPEG. No SD card needed.


## Build and run

```bash
idf.py set-target esp32p4
```

The JPEG lands in `shot/ov64a40.jpg`. `capture.py` owns the serial port for
the whole run — flashing, resetting, and reading the framed payload back out —
so close any other monitor first or esptool fails with `Access is denied`.

Board defaults target the FireBeetle AI ESP32-P4: camera SCCB on I2C port 0,
SCL `GPIO8` / SDA `GPIO7`, and no reset or power-down routed (the Pi 15-pin CSI
connector carries neither, and the sensor free-runs on its own oscillator, so
there is no host XCLK either). Re-check `CAM_*` in
[`main/ov64a40_snapshot_main.c`](main/ov64a40_snapshot_main.c) for a different
board.

## Reading the output

```
ov64a40: detected OV64A40, PID=0x0219
ov64a40: set format: MIPI_2lane_24Minput_RAW10_1640x1232_30fps
ov64a40_snapshot: AE headroom: exposure 4..1759 lines
ov64a40_snapshot: AE headroom: gain menu 0..41, max 10667 milli
ov64a40_snapshot:    t (ms) | luma | exp | gain_idx
ov64a40_snapshot:        50 |   34 | 1322 | 12
ov64a40_snapshot:        90 |   34 | 1322 | 41
ov64a40_snapshot:       120 |   42 | 1322 | 41
ov64a40_snapshot: AE settled: luma 34 -> 42, exposure 1322 lines, gain index 41
```
## Colour is not calibrated

The CCM in the OV64A40 IPA config is the **identity matrix**. The IMX708's tuned
matrix is deliberately not copied across: a colour matrix is a per-sensor,
per-CFA measurement, and a borrowed one would look like tuning while being an
unmeasured guess. Expect flat, slightly green-cast colour until it is measured
against a colour chart. The NoIR variant makes that worse and harder — with no
IR-cut filter, infrared contaminates all three channels, so any calibration has
to be done under the light the camera will actually work in.

The AWB `rg`/`bg` bounds are also deliberately wider than the IMX708's, for the
same reason: those were narrowed around a white point measured on that module,
and reusing them here would fail in the worse direction — if no pixel falls
inside the box, `min_counted` is never reached and AWB simply never updates,
which reads as a colour cast rather than as a broken config.

## Switches in the source

| Macro | Default | What it does |
| ----- | ------- | ------------ |
| `AIM_SECONDS` | 4 | settling time before the keeper frame. Raise it if the trace is still moving when the window closes |
| `IMAGE_OUT_SERIAL` | 1 | send the frame down the console as JPEG |
| `IMAGE_OUT_SD` | 0 | also write a 24-bit BMP to `/sdcard/ov64a40.bmp`; costs ~8 s |
| `TEST_PATTERN` | 0 | sensor colour bars. Clean bars mean the MIPI/CSI/ISP transport is healthy and a bad picture is optics or tuning |
| `POISON_BUFFERS` | 0 | fill capture buffers with `0xA5` first and count survivors. Proves a frame is live DMA rather than untouched PSRAM — worth having because untouched PSRAM averages mid-scale and is easy to mistake for a real exposure |
