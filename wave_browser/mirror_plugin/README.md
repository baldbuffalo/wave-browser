# Mirror Screen (Aroma plugin)

Mirror Screen is a standalone WUPS/Aroma plugin for Wave Browser. It captures
the Wii U **TV output** and serves a browser-based MJPEG receiver over the
local network. Phones, tablets, laptops and desktops can connect through a
compatible browser; no PC helper app, capture card, USB connection or
particular receiver model is required.

## Use

1. Install `MirrorScreen.wps` in
   `sd:/wiiu/environments/aroma/plugins/`.
2. Reboot the Wii U so Aroma loads the plugin.
3. Keep the Wii U and receiving device on the same Wi-Fi/LAN.
4. Find the Wii U IPv4 address in its Internet connection details.
5. Open `http://<WII-U-IP>:8088/` on the receiving device.

## Initial-version limits

- TV picture only. GamePad-screen capture is not implemented.
- Video only; audio streaming is not implemented yet.
- One receiver at a time.
- 428×240 MJPEG at a conservative capture rate; encoding uses Wii U CPU.
- GX2 capture timing and compatibility require real-console validation.
- Intended for a trusted local network. Do not expose port 8088 publicly.

## Attribution and license

The GX2 TV-capture and software-JPEG encoder are adapted from
[DoperDodge/Wii-Sec-U](https://github.com/DoperDodge/Wii-Sec-U)'s
`wsu-stream` component, licensed GPL-3.0-or-later. Those adapted files
retain that license. The HTTP receiver and Wave Browser integration are new.
See `COPYING`.
