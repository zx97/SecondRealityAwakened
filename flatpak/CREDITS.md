# Second Reality: Reawakened — Sources and Credits

**Second Reality: Reawakened** is a modern C++ remaster of **"Second Reality"**
(1993) by Future Crew. It is a continuation of **Second Reality++** by
**Jean-Sébastien Royer (XorJS)**, which itself builds on the Win32 port by
Gargaj / Conspiracy and on the original sources released by Future Crew. Every
upstream project is acknowledged below. The same file is shipped inside the
Flatpak package.

## Lineage

```
Second Reality (1993) — Future Crew
        │  original sources, released into the public domain
        │  https://github.com/mtuomi/SecondReality
        ▼
Second Reality — Win32 port (Gargaj / Conspiracy)
        │  https://github.com/ConspiracyHu/SecondRealityW32
        ▼
Second Reality++ (2025) — Jean-Sébastien Royer "XorJS"
        │  C++ port, modern toolchain, Windows / Linux / WebAssembly
        │  https://github.com/XorJS/SecondRealityPlusPlus   ·   https://www.jsr-productions.com/secondreality.html
        ▼
Second Reality: Reawakened (this project, by Manuel FLURY)
```

## Original production — Future Crew (1993)

- **Code**: Sami "PSI" Tammilehto, Mika "Trug" Tuomi, Arto "Wildfire" Vuori
- **Music**: Purple Motion (Jonne Valtonen), Skaven (Peter Hajba)

## Upstream sources

| Source | Author | URL |
|---|---|---|
| Second Reality (original demo, 1993) | Future Crew | |
| Original source code | mtuomi / Future Crew | https://github.com/mtuomi/SecondReality |
| Win32 port | Gargaj / Conspiracy | https://github.com/ConspiracyHu/SecondRealityW32 |
| **Second Reality++** — the C++ base this project continues | **Jean-Sébastien Royer (XorJS)** | https://github.com/XorJS/SecondRealityPlusPlus |

## Bundled third-party components

| Component | Author / project | License | URL |
|---|---|---|---|
| st3play (S3M replay) | Olav "8bitbubsy" Sørensen — based on the original ASM sources by Sami "PSI" Tammilehto (Future Crew), used with permission | see upstream | https://16-bits.org |
| stb_image / stb_truetype | Sean Barrett (nothings/stb) | MIT / public domain | https://github.com/nothings/stb |
| xBRZ (pixel upscaling) | Zenju | GPL-3.0 | https://sourceforge.net/projects/xbrz/ |
| Liberation Serif font | Red Hat / Liberation Fonts | SIL OFL 1.1 | https://github.com/liberationfonts |

## Runtime dependencies (provided by the Flatpak runtime)

| Component | License | URL |
|---|---|---|
| SDL2 | zlib | https://libsdl.org |
| Mesa (libGLESv2) | MIT | https://mesa3d.org |
| Freedesktop SDK / Platform | LGPL-2.1+ / various | https://freedesktop-sdk.io |

## Other tools used to produce the assets

- **Upscayl** — AI image upscaling of the bitmap assets — https://upscayl.org

## People

- **Jean-Sébastien Royer (XorJS)** — author of Second Reality++, the C++ base this
  project continues.
- **Manuel FLURY** — Second Reality: Reawakened (this project).

## Licensing notes

- The **source code** of this project is released into the **public domain**
  (Unlicense), preserving Future Crew's original public domain dedication. See
  `LICENSE.md`.
- The **compiled binary** is covered by the **GPL-3.0**. This is forced by
  **xBRZ** (GPL-3.0, used for frame upscaling), which is statically linked into
  the executable and therefore makes it a *combined work* under xBRZ's
  **copyleft**: the binary must ship with its source and the GPL-3.0 text, and
  any redistribution or modified version must remain GPL-3.0 — it cannot be
  relicensed as proprietary or public domain. Removing xBRZ would remove this
  constraint. The GPL-3.0 text ships with the package as `xbrz-GPL-3.0.txt`.

## More information

- Second Reality++ (base project): https://github.com/XorJS/SecondRealityPlusPlus
- Project page: https://www.jsr-productions.com/secondreality.html
