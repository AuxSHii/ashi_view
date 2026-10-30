# Phase 4 — The Unicode Half-Block Rasterizer & ANSI State Machine

## 1. Goal

In this phase, you will build the primary universal rendering engine of `ashi_view`: the **Unicode Half-Block TrueColor Rasterizer** (`render_halfblock.h` and `render_halfblock.c`).

these names will make sense as you read more, but to put simply : to create a rectangular grid of unicode half blocks [ like `▀`] which are colored somehow.

By pairing the Unicode upper half-block character (`▀`) with 24-bit ANSI foreground and background colors, you will achieve 2× vertical subpixel density, turning any standard terminal into a high-fidelity image display. You will also implement an **ANSI escape deduplication state machine** and a 64KB I/O buffer to deliver blazing-fast rendering speeds without terminal emulator lag.


---

## 2. What You Should Understand Before Coding


### what is unicode and why [you can skip some theory but its always good to atleast have an idea about this]

unicode is a universal coding standard designed to give every single character, letter, symbol, and emoji across the world a unique number, no matter the computer platform, program, or language.
in the early days of computing, computers used a system called ascii, which could only store 128 characters—basically just standard english letters, numbers, and basic punctuation. if you wanted to display special symbols, math operators, or characters from other languages like japanese, arabic, or hindi, old computers would break and display garbled text because they lacked a shared system.
unicode fixed this by creating a massive, single directory that currently maps well over 100,000 unique characters. it allows computers everywhere to agree that a specific number always represents the same exact thing. in our tool, we use unicode because standard ascii doesn't have the special half-block shapes (▀ and ▄) we need to trick the terminal into drawing pixels.[more on that next]

### why unicode here?

unicode is used in this ashi_view , 
 when a user is running a standard terminal environment that doesn't support direct graphic rendering protocols like kitty or iterm2. when someone uses a regular console, tmux, or connects over ssh, the tool cannot send raw image data directly to the screen. instead, it falls back to a clever trick that converts the image into text blocks, and this is exactly where unicode comes into play.


we use specific unicode characters—specifically the upper half-block ▀ and the bottom half-block ▄—to solve a physical limitation of terminal text fonts. normally, terminal characters are twice as tall as they are wide. if you tried to display a square pixel using a full solid block character, the image would look squashed and heavily distorted.

by printing the unicode upper half-block ▀, we split a single tall character cell into two perfect, square subpixels. we can then use standard terminal text commands to set the foreground color for the top half and the background color for the bottom half. unicode is what gives us the building blocks to double our vertical resolution and display sharp, proportional images out of pure text.


### what is raster and why its needed here:

a raster in comp sci and graphics is a rectangular grid of discrete picture elements called pixel where each individual cell stores specific color, intensity, or data values.

things to know ;)

• Pixels: the smallest addressable units of a raster grid, holding individual color components (such as Red, Green, and Blue—RGB)

• Resolution: defined by the grid's dimensions (number of columns by number of rows) or density (pixels per inch / PPI).

• Resolution-Dependent: unlike vector graphics (which use mathematical formulas), raster images lose sharpness and become pixelated or blurry when stretched or enlarged beyond their original grid size

• Common Formats: Includes lossy and lossless file types like JPEG, PNG, GIF, TIFF, and PSD[our targets]

Rasterization in Computer Graphics

• conversion process: rasterization is the algorithmic stage in the 3D rendering pipeline where continuous geometric models (like polygons, 3D triangles, or lines) are converted into discrete screen-space pixels.
• algorithms: Uses specific mathematical procedures like Bresenham's line algorithm or midpoint circle algorithm to map shapes onto the pixel grid.
• frame buffer: The resulting pixel values are stored in a frame buffer before being refreshed and displayed on a monitor.

read more on wiki:

https://en.wikipedia.org/wiki/Rasterisation

### how we use raster?

raster = flat array which holds the pixel data[rgba] of the scaled image we created after loading the source img.


so here is how we use a raster in this specific file.
 the term pixels in the code refers to a flat array in memory that holds all the color data for the image.[which we already have in our dt] because a terminal can only show text characters, we have to treat the terminal screen as a custom grid of pixels. since terminal characters are taller than they are wide, we look at... the raster data "two vertical rows at a time.""
 as previously discussed.

in function render_frame_halfblock that we are gonna write, we run a nested loop that moves down the rows and across the columns of the image. inside this loop, we calculate the memory offset using the formula (y * cols + c) * 4. this points us directly to the red, green, blue, and alpha values for both a top pixel and a bottom pixel.

in function blend_pixel that we are gonna write, we check if those pixels have transparency. if they do, we use math to blend the pixel colors with a solid or checkerboard background before drawing. once we have the clean colors, we use standard ansi text codes to paint the top color as the foreground and the bottom color as the background, then.. we print a half-block character.

we needed this raster approach in the first place because an image file is made of thousands of tiny squares, but a terminal only understands rows of letters. by resizing the image into a specific raster grid size that matches the terminal window,

we can convert those raw color bytes into colored text blocks that trick your eyes into seeing a real photo...[cool ...very cool indeed]

thats pretty much the gist of it , lets talk more about concepts and techniques invoved

### about use of hex:

when you see \x inside the code, it stands for hexadecimal (hex for short). it is a direct way for a programme to tell the computer exactly what raw bytes of data to send down the pipeline, rather than typing out human text.
in c, if you write a normal letter like "a", the computer understands it perfectly. but if you want to write a special symbol from the Unicode Standard like our half-blocks—you cannot easily type that with a standard keyboard. instead, we use \x followed by two numbers or letters to represent the exact raw byte value.

look in code:

• \xe2\x96\x80 is the exact byte sequence for the upper half-block ▀ character.
• \xe2\x96\x84 is the exact byte sequence for the lower half-block ▄ character.
• \x1b is the raw hex value for the escape code, which tells the terminal window that a color control command is coming instead of normal printable text.

lets talk more about tha blocks & rendring them:

### The Double-Vertical Pixel Trick (`▀`)
In Unicode, character `U+2580` is the **Upper Half Block** (`▀`, encoded in UTF-8 as the three bytes `\xe2\x96\x80`):

now the 2 terms glyph and charcters:

 a character is an abstract concept or the semantic meaning of a letter, while a glyph is the actual visual representation or drawing of that character
* The top half of the character cell is filled by the glyph[unicode char] , which takes the **Foreground** color.
* The bottom half is empty, revealing the cell's **Background** color.

By emitting:
```
\x1b[38;2;<R1>;<G1>;<B1>m \x1b[48;2;<R2>;<G2>;<B2>m ▀
```
you render two independent vertical pixels inside a single character cell:
* Pixel $(x, 2y)$ is the top subpixel (Foreground).
* Pixel $(x, 2y + 1)$ is the bottom subpixel (Background).

Because monospace fonts have a ~1:2 aspect ratio, splitting a cell into two vertical halves creates subpixels with a **1:1 square aspect ratio**.


### basics of coloring our unicode characters

in a text terminal, 
foreground refers to the color of the text character itself, 
while
background refers to the color of the solid rectangular box behind that text character.

we use both of them together in this project to turn text into a graphic screen. because a standard terminal character is twice as tall as it is wide, we print a special unicode half-block symbol ▀ that fills only the top half of the space.

a pretty standard procedure:
by setting the foreground color to match the top pixel of our image and the background color to match the bottom pixel of our image, the terminal displays two distinct, perfectly square colors inside a single letter slot. this doubles our vertical resolution and lets us display clear, smooth pictures out of raw text.



### TrueColor (24-bit ANSI Escapes)
TRUECOLOR : a standard terminal extension that allows specifying arbitrary 24-bit RGB colors (16.7 million colors) rather than being limited to the classic 16 or 256 color palettes:

* Foreground: `\x1b[38;2;R;G;Bm`
* Background: `\x1b[48;2;R;G;Bm`
* Reset to default: `\x1b[0m` (or `\x1b[39m` for default FG, `\x1b[49m` for default BG).

### The Escape Bandwidth Bottleneck & Differential State Machine

A naive renderer emits both foreground and background escape sequences for every cell:
$$\sim 40 \text{ bytes per cell} \times 12,800 \text{ cells} = 512 \text{ KB per frame!}$$
Terminals spend significant CPU time parsing escape strings. When thousands of identical escapes are sent, the terminal lags.

#### The Solution: A State Machine
Maintain the currently active terminal colors in state variables:
```c
int cur_fg_r = -1, cur_fg_g = -1, cur_fg_b = -1;
int cur_bg_r = -1, cur_bg_g = -1, cur_bg_b = -1;
```
* If the top pixel has the same color as `cur_fg`, **do not emit a foreground sequence**.
* If the bottom pixel has the same color as `cur_bg`, **do not emit a background sequence**.
* On photographic images, skies, gradients, and solid areas, this reduces the output payload by **60% to 85%**.

#### and let me tell you :
that this technique is absolutely a standard practice when writing high-performance cli tools. in the world of terminal apps and text user interfaces (tuis), this is most commonly referred to as differential rendering (or frame diffing).

i am telling you this , so you know that you dont have to invent everything from scratch , there are already great ways to do something out there, search for it , understand it while implementing it.

the skill is not to always create the solution from scrath , but to find parts of solution  implement them with your own manner , in a creative and efficient way.
efficient to me is mostly a relative term , theres always something more efficient that the other.

okk lets move on.. to topic at hands.

when people specifically talk about managing the active color states to avoid printing redundant bytes, they call it an ansi state tracking machine or color caching.

you will find this implemented under the hood of several major tools:
• terminal user interface libs:  modern libs like Ratatui (writtn in rust) or old-school c libs like ncurses rely entirely on differential rendering. they keep a virtual screen buffer of what should be on the screen, compare it against what is already there, and only send changes.
• popular cli media tools: command-line video players and image viewers like chafa, timg, and mpv (when used directly in a console mode)[i also wrote a batch script for its low end alternative , check out my github] use this state tracking method to stream fluid video frames without causing severe lag or breaking the terminal emulator's text parser.

we use it here because terminal parsers are notoriously slow at translating text strings into graphical color states. if we blindly spit out raw codes for every single box, we cause ..

bandwidth choke i.e. {when a system tries to send more data down a communication pipeline than it can handle at once, causing the data to back up and the program to slow down or lag}.

 by tracking the current states in cur_fg and cur_bg, we reduce the workload dramatically, leaving just a thin, clean stream of data for the terminal to decode.

### Transparency Compositing
When a pixel has an alpha value $A < 255$:
* **Checkerboard Mode (`--bg checker`):**
  A 2×2 subpixel checkerboard pattern alternates between two dark neutral grays (`#242424` and `#383838`).
  Spatial pattern formula:
  $$\text{checker} = \left(\left(\frac{x}{2}\right) \oplus \left(\frac{y}{2}\right)\right) \ \& \ 1$$
  Alpha blending formula:
  $$C_{\text{out}} = \frac{C_{\text{src}} \times A + C_{\text{bg}} \times (255 - A) + 127}{255}$$
* **Native Terminal Pass-Through (`--bg none`):**
  If a pixel is fully transparent ($A < 32$), we do not blend it. Instead:
  * If both subpixels are transparent: output space `' '` with reset color.
  * If top is opaque and bottom is transparent: output `▀` with `\x1b[49m` (default BG).
  * If top is transparent and bottom is opaque: output `▄` (`\xe2\x96\x84`, Lower Half Block) with `\x1b[49m`.
  The user's actual terminal background color shines through.

---

## 3. Technologies / Libraries / APIs Used

### We write ourselves
* `include/render_halfblock.h`: Interface for the half-block renderer.
* `src/render_halfblock.c`: Implementation of the 64KB output buffer, alpha blending, state machine, and cell rasterizer.

### Libraries / dependencies
* *None.* (Uses our `resample.h` and `terminal.h`).

### Platform / runtime APIs
* Standard C bulk I/O: `fwrite(buffer, 1, length, stdout)`.

#### about the function used
the function fwrite(buffer, 1, length, stdout) is a standard c lang lib function used to write raw data bytes directly to the terminal screen.

the parameters it takes:
• buffer: this is the memory location where our text and color codes are stored.
• 1: this tells the system that each item we are writing is exactly 1 byte long.
• length: this is the total number of bytes currently sitting inside our buffer that need to be sent out.
• stdout: this stands for standard output, which means the text terminal window itself.

the main purpose of this function in ashi_view is speed. in standard programming, you usually use a function like printf to show text on the screen. printf is slow because it has to look through your text, format variables, and send data to the terminal one tiny piece at a time.

if you do that for thousands of colored pixels on a screen 60 times a second, your program will freeze, lag, or tear.

instead, we created our custom OutBuf system. in function outbuf_append that we are gonna write, we pack all our raw text, unicode shapes, and \x color codes into a massive 65,536-byte tray in memory. then, in function outbuf_flush that we are gonna write, we use fwrite to dump that entire tray to the screen in one single, lightning-fast action. it bypasses all formatting overhead and draws the whole frame instantly.

it can be a lot confusing , but let it sink in &  you will get it eventually


### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `include/render_halfblock.h`: Declares `ashi_render_halfblock()`.
* `src/render_halfblock.c`:
  * `OutBuf`: Stack-allocated 64KB buffer with `outbuf_flush` and `outbuf_append`.
  * `blend_pixel`: Alpha blending for checkerboard, solid, and transparent pass-through.
  * `render_frame_halfblock`: The dual-pixel scanning loop with state deduplication.
  * `ashi_render_halfblock`: Main entry point computing fit dimensions, allocating resampled buffers, and invoking the renderer.

---

## 5. Major Tasks

### Task 1 — Design the 64KB Bulk I/O Buffer
- 1.1 Create `OutBuf` struct with fixed 64KB char array and length counter.
- 1.2 Implement `outbuf_append` that flushes via `fwrite` whenever the buffer fills.
- 1.3 Implement fast formatting helpers for foreground and background ANSI escapes.

### Task 2 — Implement Alpha Blending & Checkerboard Logic
- 2.1 Implement `blend_pixel` function supporting `BG_CHECKER`, `BG_SOLID`, and `BG_NONE`.
- 2.2 Calculate 2×2 subpixel checkerboard using bitwise XOR.
- 2.3 Implement integer alpha compositing arithmetic.

### Task 3 — Implement the Half-Block Rasterizer with State Deduplication
- 3.1 Loop over rows $r = 0 \dots \text{rows}-1$, setting $y_{\text{top}} = 2r$ and $y_{\text{bot}} = 2r + 1$.
- 3.2 Initialize state tracking variables (`cur_fg`, `cur_bg`) at the start of each row.
- 3.3 Scan columns $c = 0 \dots \text{cols}-1$, blending top and bottom subpixels.
- 3.4 Select characters (`▀`, `▄`, `' '`) based on transparency states.
- 3.5 Emit ANSI color escapes only when colors change.
- 3.6 Reset colors (`\x1b[0m\n`) at the end of each row and flush remaining buffer bytes.

### Task 4 — Connect to CLI & Verify
- 4.1 Update `src/main.c` to dispatch to `ashi_render_halfblock()`.
- 4.2 Test with opaque JPEGs and transparent PNGs.

---

## 6. Detailed Implementation Tasks

### Task 1 — Design the 64KB Bulk I/O Buffer
#### 1.1
In `src/render_halfblock.c`, define:
```c
typedef struct {
    char data[65536];
    size_t len;
} OutBuf;
```
#### 1.2
Implement `outbuf_flush(OutBuf *b)`:
If `b->len > 0`, call `fwrite(b->data, 1, b->len, stdout)` and set `b->len = 0`.
#### 1.3
Implement `outbuf_append(OutBuf *b, const char *str, size_t n)`:
If `b->len + n >= sizeof(b->data)`, call `outbuf_flush(b)`. Then `memcpy` and update `b->len`.
#### 1.4
Write helpers `outbuf_fg_rgb` and `outbuf_bg_rgb` using `snprintf` into a small temporary stack string and append.

### Task 2 — Implement Alpha Blending & Checkerboard Logic
#### 2.1
Define `blend_pixel`:
Parameters: `const uint8_t *pixel`, `int x`, `int y`, `const ashi_bg_color_t *bg`, output RGB pointers, and `bool *is_trans`.
#### 2.2
If `bg->mode == BG_NONE` and `pixel[3] < 32`:
Set `*is_trans = true` and return.
#### 2.3
If `pixel[3] == 255`, copy RGB directly and return.
#### 2.4
If blending is required:
* If `BG_CHECKER`: calculate `checker = ((x >> 1) ^ (y >> 1)) & 1`. Set background to `0x38` if true, `0x24` if false.
* If `BG_SOLID`: set background to `bg->r`, `bg->g`, `bg->b`.
* Calculate: `out = (src * a + bg * (255 - a) + 127) / 255`.

### Task 3 — Implement the Half-Block Rasterizer
#### 3.1
In `ashi_render_halfblock(img, cfg)`:
* Query terminal size with `terminal_get_size(&term_cols, &term_rows)`.
* Compute fit dimensions using `ashi_calc_fit_dimensions(img->width, img->height, max_cols, max_rows, 2.0, &cols, &rows)`.
* Allocate scaled frame buffer of size `cols * (rows * 2) * 4`.
* Downsample using `ashi_resample_rgba()`.
#### 3.2
Implement the rasterizer scanning loop:
```
for row = 0 to rows - 1:
    y_top = row * 2
    y_bot = row * 2 + 1
    reset cur_fg and cur_bg trackers to -1

    for col = 0 to cols - 1:
        blend top subpixel (col, y_top)
        blend bottom subpixel (col, y_bot)

        handle transparency combinations:
        case 1: both transparent -> print ' '
        case 2: top transparent, bottom opaque -> print '▄' (FG = bottom)
        case 3: top opaque, bottom transparent -> print '▀' (FG = top)
        case 4: both opaque -> print '▀' (FG = top, BG = bottom)
```
#### 3.3
In each case, check if target color equals current cached color. Only emit escape sequences on mismatch, then update cached color.
#### 3.4
At the end of each line, emit `\x1b[0m\n`. After the loop, flush `OutBuf` and free scaled memory.

---

## 7. Pseudocode

### Pixel Blending Logic (src/render_halfblock.c)

```
function blend_pixel(pixel, x, y, bg_config, out_r, out_g, out_b, out_is_transparent):
    alpha = pixel.alpha

    if bg_config.mode == BG_NONE and alpha < 32:
        *out_is_transparent = true
        return

    *out_is_transparent = false
    if alpha == 255:
        *out_r = pixel.r
        *out_g = pixel.g
        *out_b = pixel.b
        return

    if bg_config.mode == BG_CHECKER:
        is_checker = ((x >> 1) XOR (y >> 1)) AND 1
        bg_col = is_checker ? 0x38 : 0x24
        bg_r = bg_col, bg_g = bg_col, bg_b = bg_col
    else if bg_config.mode == BG_SOLID:
        bg_r = bg_config.r, bg_g = bg_config.g, bg_b = bg_config.b
    else:
        bg_r = 0, bg_g = 0, bg_b = 0

    inv_alpha = 255 - alpha
    *out_r = (pixel.r * alpha + bg_r * inv_alpha + 127) / 255
    *out_g = (pixel.g * alpha + bg_g * inv_alpha + 127) / 255
    *out_b = (pixel.b * alpha + bg_b * inv_alpha + 127) / 255
```

### Double-Pixel Rasterizer & State Machine (src/render_halfblock.c)

```
function render_frame_halfblock(pixels, cols, rows, bg_config, outbuf):
    for r from 0 to rows - 1:
        y_top = r * 2
        y_bot = r * 2 + 1

        cur_fg_r = -1, cur_fg_g = -1, cur_fg_b = -1
        cur_bg_r = -1, cur_bg_g = -1, cur_bg_b = -1
        cur_bg_default = false

        for c from 0 to cols - 1:
            p_top = pixels[y_top, c]
            p_bot = pixels[y_bot, c]

            blend_pixel(p_top, c, y_top, bg_config, &tr, &tg, &tb, &top_trans)
            blend_pixel(p_bot, c, y_bot, bg_config, &br, &bg_col, &bb, &bot_trans)

            if top_trans and bot_trans:
                if not cur_bg_default:
                    append(outbuf, "\x1b[0m")
                    cur_fg = -1, cur_bg = -1, cur_bg_default = true
                append(outbuf, " ")

            else if top_trans:
                // Only bottom is colored: emit lower half block
                if cur_fg != (br, bg_col, bb):
                    emit_fg_rgb(outbuf, br, bg_col, bb)
                    cur_fg = (br, bg_col, bb)
                if not cur_bg_default:
                    append(outbuf, "\x1b[49m")
                    cur_bg = -1, cur_bg_default = true
                append(outbuf, "▄")  // \xe2\x96\x84

            else if bot_trans:
                // Only top is colored: emit upper half block
                if cur_fg != (tr, tg, tb):
                    emit_fg_rgb(outbuf, tr, tg, tb)
                    cur_fg = (tr, tg, tb)
                if not cur_bg_default:
                    append(outbuf, "\x1b[49m")
                    cur_bg = -1, cur_bg_default = true
                append(outbuf, "▀")  // \xe2\x96\x80

            else:
                // Both subpixels opaque
                if cur_fg != (tr, tg, tb):
                    emit_fg_rgb(outbuf, tr, tg, tb)
                    cur_fg = (tr, tg, tb)
                if cur_bg != (br, bg_col, bb):
                    emit_bg_rgb(outbuf, br, bg_col, bb)
                    cur_bg = (br, bg_col, bb)
                    cur_bg_default = false
                append(outbuf, "▀")

        append(outbuf, "\x1b[0m\n")

    flush(outbuf)
```

---

## 8. Build Order

1. Create `include/render_halfblock.h`.
2. Implement `OutBuf` and buffer append/flush helpers in `src/render_halfblock.c`.
3. Implement `blend_pixel()`.
4. Implement `render_frame_halfblock()` with state tracking.
5. Implement `ashi_render_halfblock()` orchestrator (calling `ashi_calc_fit_dimensions` and `ashi_resample_rgba`).
6. Update `Makefile` with `src/render_halfblock.c`.
7. Connect to `src/main.c` and compile.

---

## 9. Testing & Verification

### Test 1 — Render Opaque Image
Run:
```bash
./ashi_view -p halfblock -w 60 photo.jpg
```
**Expected Observation:** The image renders crisply inside your terminal without line wrapping. The colors are accurate, and rendering finishes instantaneously without cursor stutter.

### Test 2 — Render Transparent PNG with Checkerboard
Run:
```bash
./ashi_view -p halfblock -w 40 -b checker samples/star_transparent.png
```
**Expected Observation:** The transparent outer corners display a dark checkerboard pattern. The glowing center blends smoothly into the checkerboard.

### Test 3 — Render Transparent PNG with Solid Color
Run:
```bash
./ashi_view -p halfblock -w 40 -b "#ffffff" samples/star_transparent.png
```
**Expected Observation:** The background is solid white.

---

## 10. Common Beginner Mistakes

* **Terminal Line-Wrapping Distortion:**
  * *Symptom:* The rendered image splits into diagonal staircases or repeats every second line.
  * *Why:* The image width in columns was larger than the actual terminal width, causing automatic terminal line-wrapping.
  * *Fix:* Ensure `max_cols` is clamped to `term_cols`.
* **Emitting UTF-8 Bytes in Non-UTF8 Consoles:**
  * *Symptom:* `▀` prints as `â–€`.
  * *Why:* Terminal is reading output as Latin-1 or ASCII.
  * *Fix:* Verify that `terminal_init()` set the console to UTF-8 (`SetConsoleOutputCP(CP_UTF8)` on Windows).
* **Forgetting Line-End Color Reset:**
  * *Symptom:* Text typed in the shell after the image viewer exits is colored with the last pixel's background.
  * *Why:* The terminal remembers the last active ANSI color until explicitly reset with `\x1b[0m`.
  * *Fix:* Always append `\x1b[0m\n` at the end of each rasterized row and call `terminal_restore()`.

---

## 11. Reference Comparison

Compare your work against the reference implementation:
* In `src/render_halfblock.c`, observe how `cur_bg_default` and `cur_fg_default` flags track whether the terminal is currently in its default color state.
* Notice that `render_frame_halfblock` takes a pre-resampled buffer so the rasterizer loop only performs memory reads and formatting.

---

## 12. Build-from-Memory Checkpoint

Without looking at the reference code:
1. What is the UTF-8 byte sequence for the Unicode Upper Half Block `▀`?
2. Explain how one character cell can display two independent vertical pixels.
3. How does caching `cur_fg` and `cur_bg` improve rendering performance?

---

## 13. Phase Completion Checklist

- [ ] `OutBuf` 64KB I/O buffer implemented with bulk `fwrite`.
- [ ] Checkerboard and solid alpha blending implemented.
- [ ] Upper half-block `▀` and lower half-block `▄` dual-pixel rasterization implemented.
- [ ] Differential state machine eliminates duplicate ANSI escapes.
- [ ] Static images render cleanly on terminal grid.
- [ ] Build-from-memory checkpoint completed.
- [ ] Git commit created.

---

## 14. Git Commit

### What to commit:
* `include/render_halfblock.h`
* `src/render_halfblock.c`
* Updated `src/main.c` and `Makefile`

### Commit message suggestion:
```
feat: implement unicode half-block rasterizer and differential ansi state machine
```

After this commit, proceed to [Phase 5 — The Animated GIF Playback Engine](file:///c:/Users/Alter-Book/Downloads/loki/c_img_mapper/guides/guide5.md).
