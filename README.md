# Second Reality: Reawakened

A modern, cross-platform remaster of **"Second Reality"** (1993), the legendary
demo by **Future Crew**, targeting Linux and WebAssembly (web browsers).

It is built on the C++ port **Second Reality++** by
**[Jean-Sébastien Royer](https://github.com/XorJS) (XorJS)** — itself derived from
the Win32 port by Gargaj / Conspiracy and the sources Future Crew released into
the public domain. The full lineage is in
[flatpak/CREDITS.md](flatpak/CREDITS.md).

## Platforms

- Linux (SDL2 + OpenGL ES)
- Web (WebAssembly; Chrome gives the best experience)

## Building

Requirements: a C++20 compiler, `make`, SDL2 and OpenGL ES 2.
For the web build, [Emscripten](https://emscripten.org).

### Linux

```sh
./buildLinux.sh          # equivalent to: make linux
```

The executable is written to `_Builds/linux/secondreality`. Install the
dependencies if needed:

- Debian/Ubuntu: `sudo apt install libsdl2-dev`
- Fedora: `sudo dnf install SDL2-devel`
- Arch: `sudo pacman -S sdl2`

### Web

```sh
emmake make web          # build
emmake make serve        # serve the build locally
```

## Running

```sh
_Builds/linux/secondreality [part] [options]
```

| key | part | key | part |
|---|---|---|---|
| `0` | Alkutekstit I | `a` | Plasmacube (PLZ) |
| `1` | Alkutekstit II | `b` | MiniVectorBalls |
| `2` | Alkutekstit III | `c` | Peilipalloscroll |
| `3` | Logo | `d` | 3D-Sinusfield |
| `4` | Glenz | `e` | Jellypic |
| `5` | Dottitunneli | `g` | Vector Part II |
| `6` | Techno (KOE) | `i` | Endpictureflash |
| `7` | Panicfake | `j` | Credits / Greetings |
| `8` | Vuori Scrolli | `k` | End Scrolling |
| `9` | Rotazoomer (Lens) | `U` | Hidden Part |

Options: `--res WxH`, `--delay N`, `--capture N`, `--listres`, `--help`.

Keys: `F11` toggle fullscreen, `PageUp`/`PageDown` previous/next part, `Esc`
quit. `Space` (pause/resume) is available with `SR_DEBUG=1`.

## Environment variables

All optional; with none set the demo behaves exactly as usual. The most useful
are `SR_DEBUG=1` (on-screen timecode, current part and music position, plus
pause with `Space`) and `SR_FPS=<n>` (frame target, 70 by default).

## Embedded assets

The demo ships no runtime data files: bitmaps and music are compiled into the
binary. The translation units for the largest bitmaps (ALKU, Techno, Lens,
Credits) are **generated at build time** from the source images in `tools/` — see
the Makefile `EMBED_*` rules and `tools/embed_*.py`. Everything else is
committed. No step is manual: `make` regenerates them as needed.

## Packaging

`flatpak/` builds a self-contained Flatpak bundle:

```sh
make flatpak
```

## Web player

Play it in a browser: **https://secondreality.pages.dev/**

> Tap or click on the page to enable audio (browser requirement).

## Video

- [YouTube — https://www.youtube.com/watch?v=gw72EIsSR4I](https://www.youtube.com/watch?v=gw72EIsSR4I)

## Appendix — the original project (for reference)

This remaster continues **Second Reality++** by Jean-Sébastien Royer (XorJS),
which itself derives from the Win32 port by Gargaj / Conspiracy and the 1993
original by Future Crew. Links to the **upstream** project, not to this one:

- Second Reality++ — https://www.jsr-productions.com/secondreality.html
- Windows x64, fullscreen — http://www.youtube.com/watch?v=6vqV1JEFuog
- Windows x86, windowed — http://www.youtube.com/watch?v=dqctyPvdK64
- Ubuntu, windowed — http://www.youtube.com/watch?v=2fx-b-zLOcc
- Web, Chrome — http://www.youtube.com/watch?v=MDkMdTUCUKE

## Credits and sources

See [flatpak/CREDITS.md](flatpak/CREDITS.md) for upstream projects, third-party
components and their licenses.

## License

- **Source code** — released into the **public domain** (Unlicense), preserving
  Future Crew's original public-domain dedication. See [LICENSE.md](LICENSE.md).

- **Compiled binary** — covered by the **GPL-3.0**. This is a **hard constraint,
  not a preference**: the executable **statically links xBRZ** (GPL-3.0, used for
  frame upscaling), which makes the binary a *combined work* that falls under
  xBRZ's **copyleft**. Concretely, the binary:

  - **must** be distributed together with its source and the GPL-3.0 text;
  - **must stay GPL-3.0** on any redistribution or modified version — it cannot
    be relicensed as proprietary or public domain while xBRZ is linked.

  Removing xBRZ would lift the constraint and let the whole binary become public
  domain again. See [flatpak/CREDITS.md](flatpak/CREDITS.md) for every bundled
  component and its license.
