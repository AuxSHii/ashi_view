# Phase 3 — Sizing Geometry & Area-Averaging Box Resampling

## 1. Goal

In this phase, you will solve the fundamental geometry and scaling problem of terminal image viewing: fitting an image of any resolution (e.g., 3840×2160) into a tiny terminal viewport (e.g., 80×40) while strictly preserving its visual proportions and sharpness.

You will implement the mathematical aspect-ratio fitter (`ashi_calc_fit_dimensions`) and an **Area-Averaging Box Filter Resampler** (`ashi_resample_rgba`) that eliminates downsampling aliasing and black-halo edge fringing on transparent PNGs.

stay with me ;) i will explain it in a bit. in simple terms.

---

## 2. What You Should Understand Before Coding

### Monospace Character Aspect Ratio
In graphical user interfaces, pixels are square (1:1 aspect ratio). In a terminal emulator:
* Standard monospace fonts have character cells that are roughly **twice as tall as they are wide** (an aspect ratio of 1:2).
* If you scale an image purely by character counts (e.g., 80 columns by 40 rows), an image that is originally square will appear squashed vertically.
* When we render in Unicode half-block mode (Phase 4), each character cell is split vertically into **2 subpixels**. This produces 2 square subpixels per cell! Therefore, effective vertical resolution is $2 \times \text{rows}$.

### Aspect-Ratio-Preserving Fit
To fit an image of size $W_{\text{img}} \times H_{\text{img}}$ inside a maximum terminal boundary $\text{max\_cols} \times \text{max\_rows}$:
1. Compute scale factor along width:
   $$S_w = \frac{\text{max\_cols}}{W_{\text{img}}}$$
2. Compute scale factor along height (taking into account subpixel density):
   $$S_h = \frac{\text{max\_rows} \times \text{cell\_aspect\_ratio}}{H_{\text{img}}}$$
3. Take the smaller of the two scales:
   $$S = \min(S_w, S_h)$$
4. Target width in columns:
   $$\text{target\_cols} = \text{round}(W_{\text{img}} \times S)$$
5. Target height in character rows:
   $$\text{target\_rows} = \text{round}\left(\frac{H_{\text{img}} \times S}{\text{cell\_aspect\_ratio}}\right)$$

### Downsampling: Nearest-Neighbor vs. Area-Averaging (Box Filter)
* **Nearest-Neighbor:**
  Picks the single source pixel closest to the destination coordinate and ignores all other pixels.
  *Downside:* When downscaling a 4000-pixel wide photo to 80 pixels, nearest-neighbor samples 1 pixel and skips 49 pixels. Thin lines vanish, diagonal lines become jagged steps (aliasing), and patterns create noisy moiré rings.
* **Area-Averaging (Box Filter):**
  Treats each destination pixel as a continuous window $[x_0, x_1) \times [y_0, y_1)$ over the source image. It computes the weighted average of *every* source pixel that falls partially or fully inside that window. The resulting image is smooth, sharp, and artifact-free.[hope so]

### Alpha-Weighted Color Accumulation
When averaging colors that include transparency, a naive average produces dark borders around cutout edges (the "black halo" bug).[i know cuz i wasted a lot of time on that]
* *Why:* Fully transparent pixels in PNGs often have RGB values of `(0, 0, 0)` (black). If you average opaque white `(255, 255, 255, 255)` with transparent black `(0, 0, 0, 0)`, you get semi-transparent dark gray `(128, 128, 128, 128)` instead of semi-transparent white!
* *Solution:* Weight RGB contributions by their alpha channel:
  $$\text{Acc}_R = \sum R_i \cdot \alpha_i \cdot w_i, \quad \text{Acc}_\alpha = \sum \alpha_i \cdot w_i$$
  $$\text{Final } R = \frac{\text{Acc}_R}{\text{Acc}_\alpha}$$



### in simple term what we are doing?
    For Reasmpling the image -  changing the pixel grid of an image to fit a new size or shape

Part 1: Downsampling (Shrinking the Image)
in the code my comment says:
"it uses 1.area averaging[box filter] with alpha weighting for downsampling"
   means:

when you shrink an image, you are squeezing many pixels into a smaller space. If you just throw away pixels, the image gets jagged and blocky (aliased). To prevent this, the code uses Area Averaging.

• The Box Filter: Imagine drawing a grid over the original large image. each new, smaller pixel corresponds to a "box" (a small region) on the original image. The code looks at all the original pixels inside that box and averages their colors together.

• Alpha Weighting: this handles transparency. if your image has a transparent background (like a PNG), the "alpha channel" dictates how see-through a pixel is. "Alpha weighting" means that fully visible pixels contribute heavily to the final average color, while almost transparent pixels are mostly ignored. This prevents ugly dark or gray outlines from appearing around the edges of transparent objects when they are shrunk.




Part 2: Upsampling (Stretching the Image)
comment also sayss:
"2. using bilinear interpolation for upsampling [even i dont completely get it]"

means:

when you blow an image up to a larger size, you are creating new pixels out of thin air. The code has to "guess" what color those new pixels should be. It uses Bilinear Interpolation to make that guess.

• how it works: instead of just copying the nearest pixel (which makes the image look like blocky Minecraft blocks), bilinear interpolation looks at the four closest surrounding pixels (2x2 grid) from the original image.

• math: it calculates a weighted average based on how close the new pixel is to each of those four original pixels.

  • If the new pixel is right next to a red pixel and far from a blue pixel, it will be mostly red.
  • If it sits right in the middle, it blends them perfectly.

• The Result: This creates smooth transitions and gradients, making the enlarged image look soft and natural rather than pixelated.

and these things we are talking about are not at all new , these are industry standards. These exact approach is used all the time in graphic design tools, game engines, video players, and image libraries (like Photoshop, FFmpeg, and OpenCV).


### lets talk a bit more in depth about it... [stay with me]

1. What is "Image Resampling"? (core)

before rwriting or reading a line of C code, you must understand what resampling actually means.
an image is just a grid of tiny color blocks called pixels. It has a fixed resolution (e.g., \(1000 \times 1000\) pixels). when you want to display that image on a screen or a window that is a completely different size, you cannot change the original file. Instead, you must build a brand new grid of pixels.

resampling is the mathamatical process of creating that new grid and calculating exactly what color should go into every new cell based on the old grid.


• if the new grid is smaller, it's called Downsampling (shrinking).

• if the new grid is larger, it's called Upsampling (stretching).

2. :what our code will do?

our code should contains two functions: ashi_calc_fit_dimensions and ashi_resample_rgba. Together, their singular job is to take a massive image and cleanly shrink it down into a tiny grid that can fit perfectly inside a terminal window without looking distorted or squished.

here's the high-lvl workflow:

1. the Grid Fit Check: itt looks at your terminal size and the image size, figures out which one is limiting, and calculates the target width and height.

2. the Size Test: it checks if the image is scaling down or up.

3. the Downsampling Engine: if shrinking, it runs an Area-Averaging Box Filter that blends blocks of pixels together, keeping track of transparency.


4. the Upsampling Engine: if expanding, it uses Bilinear Interpolation to smoothly blend the four nearest pixels together so the image doesn't look like giant blocks.

3. : Deep-Dive-

let's look at the three major segments of our system.
[you can come to it later when uh attempted it youself or can read now]

1. The Terminal Display & Aspect Ratio Fit (ashi_calc_fit_dimensions)

• simple logic:
terminal text characters are not square; they are tall rectangles (usually twice as tall as they are wide). [as said above] . if you mapped one pixel to one text character, your image would look stretched vertically. this function fixes that. It scales the image down to fit your terminal boundaries while compensating for the rectangular shape of text slots.

• how its gonna do that?
the code should reads cell_aspect_ratio. 
It scales the height into a virtual "subpixel space" by multiplying the maximum terminal rows by the aspect ratio. 
it finds the bounding scale factor (scale_w vs scale_h) and takes the smaller one (scale) to ensure the image fits entirely within the terminal.

 finally, it divides the calculated height back by the cell_aspect_ratio so the final output dimensions (out_cols, out_rows) line up exactly with text rows and columns without stretching the image content.

2. Downsampling: Area-averaging with Alpha weighting

• simple logic:

when shrinking an image down to terminal size, a single terminal character cell might cover a block of \(20 \times 20\) original pixels. Instead of just picking one pixel and throwing away the other 399, our code draws a boundary box around all 400 pixels, looks at their colors, and averages them together.

also, it uses Alpha Weighting. if a pixel is transparent (alpha = 0), its color is completely ignored. [makes sense]

 if it is fully opaque (alpha = 255), it counts fully. This prevents transparent edges from picking up hidden background color values (which usually causes a dark, ugly outline or "halo" effect around shapes).[which we talked about above]

• how its gonna do that? (look at Loops):
  • the Bounding Box (y0, y1, x0, x1): 
  the code maps the destination character coordinate (dx, dy) back to a floating-point box on the source image.

   itt takes the floor (int ix0 = (int)x0) and the ceiling (int ix1 = (int)ceil(x1)) to find every source pixel that is even partially inside that box.


  • partial pixel weights (x_weight, y_weight):

   if a source pixel sits right on the edge of our box, the code calculates exactly what fraction of that pixel is inside using fmin and fmax. The product w = x_weight * y_weight is the exact geometric area coverage of that specific pixel.[yeah thats right]


  • the Accumulation : For every pixel in the box, the color channels (\(R, G, B\)) are multiplied by both their transparency (a) and their fractional area weight (w).
\(\text{r\_acc}+=\text{pixel\_R}\times a\times w\)
 
  • the Normalization: At the end of the pixel block, it divides the color accumulators by a_acc (the total weight of transparency) rather than just the area weight.

it cleans up the color values before saving them into the destination array (dst).

3. Upsampling: Bilinear Interpolation

• simple logic:

if a user zooms way into the terminal display, a single terminal character might point to a fractional spot between pixels (like pixel position \(4.3\)).

 the code cannot look up a fractional pixel. Instead of blindly rounding to pixel \(4\), it grabs the four closest integer pixels surrounding that fractional spot. it computes a smooth gradient blend based on how close the target spot is to each of those four neighbors.

• how its gonna do that?:
  • Center Alignment (+ 0.5 and - 0.5): The code aligns pixel centers rather than top-left corners so the image scales cleanly without shifting to the side.

  • Ffnding the neighbors (y0, y1, x0, x1): It drops a coordinate pointer (sx, sy) onto the source image. floor(sx) gives the top-left neighbor index (x0). Adding 1 gives the right-hand neighbor index (x1).

  • calculating the interpolation weights (wx, wy): wx is the fractional distance between the pixels horizontally. The code calculates four distinct weights:
    • w00 = (1.0 - wx) * (1.0 - wy) (Top-Left weight)
    • w10 = wx * (1.0 - wy) (Top-Right weight)
    • w01 = (1.0 - wx) * wy (Bottom-Left weight)
    • w11 = wx * wy (Bottom-Right weight)
 
  • the final blend: will hav to loops through all 4 channels (\(R, G, B, A\)) using the variable c. will performs a sum of products:


\(\text{Value}=(P_{00}\times w_{00})+(P_{10}\times w_{10})+(P_{01}\times w_{01})+(P_{11}\times w_{11})\)
it clamps the final floating point calculation between 0.0 and 255.0 using fmin/fmax and adds 0.5 to cleanly round it to the nearest integer byte (uint8_t).


---

## 3. Technologies / Libraries / APIs Used

### We write ourselves
* `include/resample.h`: Interface for aspect-ratio calculations and image resampling.
* `src/resample.c`: Implementation of the area-averaging box filter and bilinear upsampling algorithm.

### Libraries / dependencies
* Standard math library (`<math.h>`): `floor()`, `ceil()`, `fmin()`, `fmax()`. Linked with `-lm`.

### Platform / runtime APIs
* Standard C memory manipulation: `memcpy()`, `memset()`.

### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `include/resample.h`: Declares `ashi_calc_fit_dimensions()` and `ashi_resample_rgba()`.
* `src/resample.c`: Contains the box-filter area accumulation loop and bilinear interpolation fallback.
* Notice in `Makefile` that `LDLIBS = -lm` is included to link the C math library.

---

## 5. Major Tasks

### Task 1 — Implement Aspect Ratio geometry
- 1.1 In `include/resample.h`, declare `ashi_calc_fit_dimensions()`.
- 1.2 In `src/resample.c`, implement the scaling math using double-precision floating point.
- 1.3 Ensure clamp checks prevent 0 or negative columns and rows.

### Task 2 — Implement the Area-Averaging Box Filter
- 2.1 Calculate continuous source coordinate boundaries $[x_0, x_1)$ and $[y_0, y_1)$ for every destination pixel.
- 2.2 Loop over all overlapping source pixels and compute horizontal and vertical overlap weights.
- 2.3 Accumulate color channels weighted by alpha and area.
- 2.4 Normalize final color values and write to destination RGBA buffer.

### Task 3 — Implement Bilinear Fallback for Upscaling
- 3.1 Detect if destination dimensions exceed source dimensions.
- 3.2 Compute fractional sampling coordinates and four-corner pixel weights.
- 3.3 Interpolate RGBA channels and store in destination.

### Task 4 — Verify Resampling via Unit Test Harness
- 4.1 Write a small test in `src/main.c` that loads an image, calculates fit dimensions for an 80×24 terminal, resamples the image, and verifies that the output buffer is populated.

---

## 6. Detailed Implementation Tasks

### Task 1 — Implement Aspect Ratio Geometry
#### 1.1
In `include/resample.h`, declare:
```c
void ashi_calc_fit_dimensions(int img_w, int img_h,
                              int max_cols, int max_rows,
                              double cell_aspect_ratio,
                              int *out_cols, int *out_rows);
```
#### 1.2
In `src/resample.c`, implement boundary safety checks: if any input dimension $\le 0$, set output to 1 and return.
#### 1.3
Compute effective height `effective_h = max_rows * cell_aspect_ratio`.
Compute horizontal scale `scale_w = max_cols / img_w` and vertical scale `scale_h = effective_h / img_h`.
Select `scale = fmin(scale_w, scale_h)`.
#### 1.4
Multiply image width and height by `scale`, round to nearest integer, and clamp between 1 and `max_cols` / `max_rows`.

### Task 2 — Implement the Area-Averaging Box Filter
#### 2.1
In `include/resample.h`, declare:

```c
void ashi_resample_rgba(const uint8_t *src, int src_w, int src_h,
                        uint8_t *dst, int dst_w, int dst_h);
```

#### 2.2
In `src/resample.c`, handle the identity case: if `src_w == dst_w` and `src_h == dst_h`, perform a direct `memcpy` and return.
#### 2.3
When downsampling (`dst_w <= src_w && dst_h <= src_h`), loop over each destination pixel `(dx, dy)`.
Compute continuous source interval:
* $y_0 = dy \cdot \frac{src\_h}{dst\_h}$, $\quad y_1 = (dy + 1) \cdot \frac{src\_h}{dst\_h}$
* $x_0 = dx \cdot \frac{src\_w}{dst\_w}$, $\quad x_1 = (dx + 1) \cdot \frac{src\_w}{dst\_w}$
#### 2.4
Determine integer boundary indices:
* $iy_0 = \lfloor y_0 \rfloor$, $\quad iy_1 = \min(src\_h, \lceil y_1 \rceil)$
* $ix_0 = \lfloor x_0 \rfloor$, $\quad ix_1 = \min(src\_w, \lceil x_1 \rceil)$
#### 2.5
Initialize accumulators `r_acc = 0.0`, `g_acc = 0.0`, `b_acc = 0.0`, `a_acc = 0.0`, `total_weight = 0.0`.
For each integer coordinate $(sx, sy)$:
* $y\_weight = \min(y_1, sy + 1) - \max(y_0, sy)$
* $x\_weight = \min(x_1, sx + 1) - \max(x_0, sx)$
* $w = x\_weight \cdot y\_weight$
* Extract source pixel RGBA.
* Accumulate:
  * `r_acc += red * alpha * w`
  * `g_acc += green * alpha * w`
  * `b_acc += blue * alpha * w`
  * `a_acc += alpha * w`
  * `total_weight += w`
#### 2.6
Compute normalized values:
* If `total_weight > 0`: `final_alpha = a_acc / total_weight`
* If `a_acc > 0`: `final_red = r_acc / a_acc` (similarly for green and blue)
* Write clamped 8-bit unsigned values into `dst[(dy * dst_w + dx) * 4]`.

### Task 3 — Implement Bilinear Fallback for Upscaling
#### 3.1
If `dst_w > src_w` or `dst_h > src_h`, use bilinear interpolation.
For each `(dx, dy)`:
* Map destination center to source: $sx = (dx + 0.5) \cdot \frac{src\_w}{dst\_w} - 0.5$, $sy = (dy + 0.5) \cdot \frac{src\_h}{dst\_h} - 0.5$.
* Find four surrounding integer neighbors $(x_0, y_0)$, $(x_1, y_0)$, $(x_0, y_1)$, $(x_1, y_1)$ clamped to image edges.
* Compute fractional weights $wx = sx - x_0$ and $wy = sy - y_0$.
* Calculate weighted sum for all 4 channels and store in destination.

---

## 7. Pseudocode

### Aspect Ratio Sizing (src/resample.c)

```
function ashi_calc_fit_dimensions(img_w, img_h, max_cols, max_rows, cell_ratio, out_cols, out_rows):
    if img_w <= 0 or img_h <= 0 or max_cols <= 0 or max_rows <= 0:
        *out_cols = 1
        *out_rows = 1
        return

    effective_height = max_rows * cell_ratio
    scale_horizontal = max_cols / img_w
    scale_vertical   = effective_height / img_h
    scale = min(scale_horizontal, scale_vertical)

    cols = round(img_w * scale)
    subpixel_h = round(img_h * scale)
    rows = round(subpixel_h / cell_ratio)

    // Clamp within terminal boundaries
    *out_cols = clamp(cols, 1, max_cols)
    *out_rows = clamp(rows, 1, max_rows)
```

### Area-Averaging Box Filter (src/resample.c)

```
function ashi_resample_rgba(src, src_w, src_h, dst, dst_w, dst_h):
    if src_w == dst_w and src_h == dst_h:
        copy_memory(dst, src, dst_w * dst_h * 4)
        return

    if dst_w <= src_w and dst_h <= src_h:
        // Downsampling pass
        for dy from 0 to dst_h - 1:
            y0 = dy * (src_h / dst_h)
            y1 = (dy + 1) * (src_h / dst_h)
            iy0 = floor(y0)
            iy1 = min(src_h, ceil(y1))

            for dx from 0 to dst_w - 1:
                x0 = dx * (src_w / dst_w)
                x1 = (dx + 1) * (src_w / dst_w)
                ix0 = floor(x0)
                ix1 = min(src_w, ceil(x1))

                r_acc = 0.0, g_acc = 0.0, b_acc = 0.0, a_acc = 0.0
                total_weight = 0.0

                for sy from iy0 to iy1 - 1:
                    y_weight = min(y1, sy + 1.0) - max(y0, sy)
                    if y_weight <= 0.0 continue

                    for sx from ix0 to ix1 - 1:
                        x_weight = min(x1, sx + 1.0) - max(x0, sx)
                        if x_weight <= 0.0 continue

                        weight = x_weight * y_weight
                        pixel = read_rgba(src, sx, sy, src_w)
                        alpha = pixel.a

                        // Alpha weighting prevents dark fringes
                        r_acc += pixel.r * alpha * weight
                        g_acc += pixel.g * alpha * weight
                        b_acc += pixel.b * alpha * weight
                        a_acc += alpha * weight
                        total_weight += weight

                if total_weight > 0.0:
                    final_a = a_acc / total_weight
                    if a_acc > 0.0:
                        final_r = r_acc / a_acc
                        final_g = g_acc / a_acc
                        final_b = b_acc / a_acc
                    else:
                        final_r = 0, final_g = 0, final_b = 0
                else:
                    final_r = 0, final_g = 0, final_b = 0, final_a = 0

                write_rgba(dst, dx, dy, dst_w, final_r, final_g, final_b, final_a)
        return

    // Bilinear upsampling pass for enlargements
    run_bilinear_interpolation(src, src_w, src_h, dst, dst_w, dst_h)
```

---

## 8. Build Order

1. Create `include/resample.h` and declare the prototypes.
2. Implement `ashi_calc_fit_dimensions()` in `src/resample.c`.
3. Test `ashi_calc_fit_dimensions()` with test cases (square image, 16:9 widescreen, portrait).
4. Implement the box filter in `src/resample.c`.
5. Implement bilinear fallback in `src/resample.c`.
6. Add `src/resample.c` to `Makefile` and ensure `-lm` is in `LDLIBS`.
7. Compile and run test script.

---

## 9. Testing & Verification

### Test 1 — Compilation with Math Library
Run:
```bash
make
```
**Expected Observation:** Clean compilation. If `round()` or `floor()` produces undefined reference errors, ensure `LDLIBS = -lm` is placed at the end of the linker command line in `Makefile`.

### Test 2 — Mathematical Fit Dimensions Test
Write a temporary test in `src/main.c`:
* Input image: 1920×1080 (16:9 widescreen).
* Maximum terminal boundaries: 80 columns × 24 rows, `cell_aspect_ratio = 2.0`.
* Call `ashi_calc_fit_dimensions(1920, 1080, 80, 24, 2.0, &cols, &rows)`.
**Expected Observation:**
* `cols = 80`
* `rows = 22` (or 23)
The image fills the width and scales the height proportionally.

### Test 3 — Downsampling Alpha Integrity
Resample a transparent test PNG (e.g., `samples/star_transparent.png`) down to 32×32 pixels.
Print out pixel values of the corners:
**Expected Observation:** Fully transparent corner pixels remain `(0, 0, 0, 0)`. Anti-aliased boundary pixels show gradual alpha transitions without turning black.

---

## 10. Common Beginner Mistakes

* **Linker Error `undefined reference to 'ceil'` or `'floor'`:**
  * *Why:* In GCC/Clang on Unix, math functions reside in `libm`.
  * *Fix:* Add `-lm` to the linker flags (`LDLIBS = -lm`).
* **Integer Division Truncation:**
  * *Symptom:* `src_h / dst_h` evaluates to 0.
  * *Why:* In C, dividing two `int` variables performs floor integer division (`100 / 200 == 0`).
  * *Fix:* Cast to `double` before dividing: `(double)src_h / (double)dst_h`.
* **Off-by-One Buffer Overflow:**
  * *Symptom:* Memory corruption or segfault during downsampling.
  * *Why:* `iy1 = ceil(y1)` can exceed `src_h` if floating-point imprecision yields `src_h + 0.00000001`.
  * *Fix:* Always clamp ceiling indices: `if (iy1 > src_h) iy1 = src_h;`.

---

## 11. Reference Comparison

Compare your work against the reference implementation:
* Look at `src/resample.c`: Check how the reference computes `x_weight` and `y_weight` by finding the overlap length using `fmin` and `fmax`.
* Look at how the reference weights RGB values by `alpha * w` instead of just `w`.

---

## 12. Build-from-Memory Checkpoint

Without looking at the reference code:
1. Explain why calculating the average color of transparent pixels requires weighting RGB values by alpha.
2. Why is `cell_aspect_ratio` set to 2.0 for Unicode half-block rendering?
3. What is the difference between nearest-neighbor sampling and area-averaging box filtering?

---

## 13. Phase Completion Checklist

- [ ] `include/resample.h` declares sizing and resampler prototypes.
- [ ] `ashi_calc_fit_dimensions()` preserves aspect ratio within terminal bounds.
- [ ] Area-averaging box filter implemented with continuous interval coverage.
- [ ] Alpha-weighted color accumulation prevents dark fringe borders.
- [ ] Bilinear fallback implemented for enlargements.
- [ ] `Makefile` links math library (`-lm`).
- [ ] Build-from-memory checkpoint completed.
- [ ] Git commit created.

---

## 14. Git Commit

### What to commit:
* `include/resample.h`
* `src/resample.c`
* Updated `Makefile`

### Commit message suggestion:
```
feat: implement area-averaging box resampler and aspect-ratio geometry
```

After this commit, proceed to [Phase 4 — The Unicode Half-Block Rasterizer & ANSI State Machine](file:///c:/Users/Alter-Book/Downloads/loki/c_img_mapper/guides/guide4.md).
