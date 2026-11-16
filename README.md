# ashi-view 
a fast, zero-dependency terminal image viewer written in c99.

`ashi_view` inspects your terminal environment and picks the sharpest way to draw images. on modern gpu-accelerated terminals like kitty, ghostty, iterm2, and wezterm, it streams full-resolution native graphics. over ssh, inside `tmux`, or on classic consoles, it falls back to a custom unicode half-block renderer with truecolor support and anti-aliased resampling.

##### wanna learn how to code this yourself?
go to [guides](guides) folder and start from guide 1 to 7...

#### features
<img width="4371" height="2844" alt="features" src="https://github.com/user-attachments/assets/4be347c4-22dd-4575-8c78-3a18ccce2c85" />


it handles transparent pngs (with checkerboard or custom hex backgrounds) and plays animated gifs without screen flicker.
here's the end to end programe flow
<img width="3267" height="5274" alt="final_flow" src="https://github.com/user-attachments/assets/87274d91-a21b-4497-9864-1f4c3b34919e" />



1. it support unix + windows
2. it support terminal protocols like kitty , iterm , windows cmd [legacy]

i will also be writing a set of guides one can follow along , learn the concepts and code this app themselves..
mostly in my guides : we have comppiled steps + theory + pseudocode for important steps and code sections..

mostly theory in these guides is me filling up the  gaps i had in my knowledge base , i googled, i used ai etc in order to fill those gaps and learn then i compiled these information too in the guides.
NOTE: i used ai in order to fill my knowledge gaps and compiled those same info in these guide files to help a beginner who will be reading it , as i myself am a learner & a beginner too.
these guides are not completely and puerly written by me...! although i wrote a major part of them but still i wanted to be transparent with anyone trusting this repo to learn something..


## how it works under the hood

### 1.terminal auto-detection & rendering backends

* **kitty graphics protocol (`kitty`, `ghostty`):** streams raw 32-bit rgba pixels directly to the terminal's gpu compositor using application program command (`\x1b_G...`) sequences. because kitty drops escape payloads longer than 4,096 base64 characters, we slice raw pixel buffers into exact 3,072-byte chunks (`3072 / 3 * 4 = 4096`) on the stack and stream them with `m=1` / `m=0` continuation flags.
* **iterm2 inline images (`iterm2`, `wezterm`, `warp`, `mintty`):** uses osc 1337 (`\x1b]1337;File=inline=1...`) sequences. if you view an unmodified image with `--bg none`, `ashi_view` skips decoding completely and base64-encodes the original file bytes straight off disk. if you apply a custom background or play a gif, it compresses frames to png in memory using a dynamically growing buffer (`MemBuffer`) before encoding.
* **unicode half-block fallback (`tmux`, `xterm`, `alacritty`, windows terminal):** standard terminal character cells are roughly twice as tall as they are wide (1:2 aspect ratio). by printing the upper half-block glyph (`▀`, u+2580), we pack two square subpixels into every single character cell—using the foreground color (`\x1b[38;2;r;g;bm`) for the top pixel and the background color (`\x1b[48;2;r;g;bm`) for the bottom pixel.


### 2. cutting ansi bloat by up to 85%
printing full truecolor escape codes for every cell on a large terminal pushes hundreds of kilobytes per frame, causing visible tearing. in `render_halfblock.c`, `ashi_view` tracks the terminal's active foreground and background rgb state. if an adjacent pixel shares the same color, we skip emitting the escape code altogether, then flush the entire frame in one bulk `fwrite` call through a 64 kb output buffer.

<img width="837" height="454" alt="halfblock double pixel" src="https://github.com/user-attachments/assets/50cae688-d422-44e1-bec8-d05aa9cb94ac" />




### 3. area-averaging box resampler
shrinking a 4k photo down to 80 columns with nearest-neighbor sampling turns fine lines into jagged noise. `resample.c` maps each target subpixel back to a fractional rectangle on the source image and averages every overlapping pixel weighted by its exact coverage area and alpha channel (preventing dark halos around transparent edges).

### 4. flicker-free gif playback
instead of clearing the screen between gif frames, `ashi_view` pre-processes frames upfront, moves the cursor back to the top-left of the image (`\x1b[<rows>A\r`), and overwrites the previous frame in place. a monotonic clock measures how long each frame took to render and subtracts that time from the sleep delay so animations never drift out of sync.
### 5. alpha blending
<img width="3302" height="3835" alt="final_alpha-blending" src="https://github.com/user-attachments/assets/2bf98a8b-9915-4f81-820f-96f89a3abda0" />


---

## building & installing

you only need a c99 compiler (`gcc`, `clang`, or msvc) and `make` or `cmake`. all third-party headers (`stb_image.h` and `stb_image_write.h`) are already bundled in `vendor/`.

**using make:**
```bash
make
sudo make install
```

**using cmake:**
```bash
cmake -B build
cmake --build build
```

---

## usage

```bash
ashi_view [options] <image-file>
```

### flags

| flag | argument | what it does |
| :--- | :--- | :--- |
| `-p`, `--protocol` | `auto`, `kitty`, `iterm`, `halfblock` | force a specific rendering backend |
| `-w`, `--width` | `<cols>` | set max width in terminal columns |
| `-h`, `--height` | `<rows>` | set max height in terminal rows |
| `-b`, `--bg` | `checker`, `black`, `white`, `none`, `#RRGGBB` | set background for transparent images |
| `-l`, `--loop` | — | loop animated gifs forever (`q` or `ctrl+c` to quit) |
| `-1`, `--once` | — | play animated gifs once and exit |
| `-n`, `--loops` | `<count>` | play animated gifs `<count>` times |
| `-s`, `--speed` | `<float>` | playback speed multiplier (e.g., `2.0`, `0.5`) |
| `-c`, `--clear` | — | clear the terminal before drawing |
| `-i`, `--info` | — | print image dimensions, channels, and detected protocol |
| `-v`, `--version` | — | print version info |
| `--help` | — | show help text |

### examples

```bash
# auto-detect terminal and fit image to window
ashi_view photo.jpg

# inspect metadata without rendering
ashi_view -i sprite.png

# blend a transparent png over a custom hex color
ashi_view -b "#1e1e2e" logo.png

# force half-block mode clamped to 60 columns
ashi_view -p halfblock -w 60 architecture.png

# play a gif twice at 1.5x speed
ashi_view -s 1.5 -n 2 spinner.gif
```

---

## project layout

```text
ashi_view/
├── Makefile                 # posix / mingw build rules
├── CMakeLists.txt           # cross-platform cmake config
├── include/                 # public headers & structs
├── src/
│   ├── main.c               # cli flag parsing & entry point
│   ├── protocol.c           # env var detection & renderer dispatch
│   ├── image.c              # png/jpg/gif decoding via stb_image
│   ├── resample.c           # area-averaging box filter & aspect fitting
│   ├── base64.c             # lookup-table base64 encoder
│   ├── render_kitty.c       # 4096-byte chunked kitty apc streamer
│   ├── render_iterm.c       # osc 1337 streamer & in-memory png writer
│   ├── render_halfblock.c   # unicode half-block & differential ansi engine
│   ├── terminal.c           # cross-platform terminal size, clocks & raw input
│   └── stb_impl.c           # single compilation unit for stb libraries
├── vendor/                  # bundled stb_image & stb_image_write
└── samples/                 # test fixtures and sample generator script
```

## license

mit license. `stb_image.h` and `stb_image_write.h` in `vendor/` are in the public domain.
