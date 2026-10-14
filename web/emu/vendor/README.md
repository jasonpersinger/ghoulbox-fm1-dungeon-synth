<!-- SPDX-License-Identifier: GPL-3.0-only -->

# Browser audio codecs

These files are pinned, self-hosted dependencies used only when the web synth exports compressed audio. They do not run in the FM-1 firmware or its audio worklet.

| File | Upstream release/source | SHA-256 |
| --- | --- | --- |
| `libflac/libflac.min.wasm.js` | libflac.js 5.6.0, commit `4c1bce76806d947de53af271c9dbb851913a633d` | `76892e974c95adc489910bc991e1d234bb48fb9f1bcc3f76ab471a1493c30f7e` |
| `libflac/libflac.min.wasm.wasm` | libflac.js 5.6.0 / libFLAC 1.3.4 | `dd145880539e836501e3c779aae19e0d3915a90fbf83b50c8a5cde819c6450b0` |
| `mp3/WasmMediaEncoder.min.js` | wasm-media-encoders 0.7.0, commit `4a45333baadbab312d1cc0911151dfad23157c51` | `dd4e17abf5377dfecc726d6ec5e7b72dab01cf3522974278e5347f4e68480fb3` |
| `mp3/mp3.wasm` | wasm-media-encoders 0.7.0 / LAME source commit `98db548e8e851defbba3184125ce10725355c332` | `85e81719250b9a667b1258143f689dda70e3e57a7e7c29ab0b4cef65c8f6eb9a` |

Upstream source and build instructions:

- <https://github.com/mmig/libflac.js/tree/4c1bce76806d947de53af271c9dbb851913a633d>
- <https://github.com/xiph/flac/tree/1.3.4>
- <https://github.com/arseneyr/wasm-media-encoders/tree/4a45333baadbab312d1cc0911151dfad23157c51>
- <https://github.com/arseneyr/lame/tree/98db548e8e851defbba3184125ce10725355c332>

The LAME WebAssembly file remains a separate, replaceable runtime module. Replace `mp3/mp3.wasm` with a compatible rebuild from the cited source to use a modified LAME build.

Licence texts are in the repository's `LICENSES/` directory and are copied to `licenses/` in the built web emulator.
