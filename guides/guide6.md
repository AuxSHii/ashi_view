# Phase 6 - High-Resolution Native Protocols (Base64, Kitty & iTerm2)

## 1. goal

in this phase, you will implement hardware-accelerated, native high-resolution graphics rendering for modern terminals. You will make:
1. a high-throughput **Base64 encoder** (`base64.h` and `base64.c`).
2. the **Kitty Graphics Protocol** backend (`render_kitty.h` and `render_kitty.c`) using APC escape sequences and 4,096-byte chunking.
3. the **iTerm2 Inline Images Protocol** backend (`render_iterm.h` and `render_iterm.c`) using OSC 1337 sequences with both raw file pass-through and in-memory PNG encoding.

by the end of this phase, running `ashi_view` inside terminals like Kitty, Ghostty, WezTerm, or iTerm2 will render native, full-resolution retina graphics rather than text blocks.

---

## 2. what you should understand before coding

### why modern terminal protocols exist
Unicode half-blocks provide universal compatibility, but they are limited to the cell resolution of the terminal (typically ~100–200 pixels across). Modern terminal emulators (Kitty, Ghostty, WezTerm, iTerm2) embed full GPU texture renderers.   they allow CLI programs to send raw pixel buffers directly to the GPU compositor for 4K / Retina display quality.  

### Base64 encoding :
BASE64 — A binary-to-text encoding scheme that translates raw 8-bit binary bytes into safe ASCII text characters (A–Z, a–z, 0–9, `+`, `/`).
it is a binary-to-text encoding scheme that converts binary data (such as images, files, or raw bytes) into a string of 64 printable ASCII characters. It ensures that data travels safely through text-based systems like email, JSON, and URLs without getting corrupted.
* **the 3-to-4 rule:**
  every 3 raw bytes (24 bits) are split into four 6-bit integers ($4 \times 6 = 24\text{ bits}$).
  each 6-bit integer (value 0–63) indexes an alphabet lookup table:  
  `"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"`  
* **padding (`=`):**
  if the input length is not a multiple of 3, the final block is padded with 1 or 2 `=` characters.  
* **output size calculation:**
  $$\text{Encoded Length} = \left\lceil \frac{\text{len}}{3} \right\rceil \times 4 = \left( \frac{\text{len} + 2}{3} \right) \times 4$$  

### kitty graphics protocol architecture
* **Escape structure:** Application Program Command (APC):
  ```
  \x1b_G<control_keys>;<payload>\x1b\\
  ```
* **transmission mode:** we use direct RGBA streaming (`f=32`).
  * `a=T`: transmit and display immediately.    
  * `s=<width>,v=<height>`: source pixel dimensions.  
  * `c=<cols>,r=<rows>`: desired grid cell placement.  
* **the 4,096-Byte chunking requirement:**
  kitty terminal buffers enforce that no single escape   sequence payload can exceed **4,096 base64 characters**.  
  since 4 base64 characters equal 3 raw bytes:
  $$\text{Max Raw Bytes per Chunk} = \frac{4096}{4} \times 3 = 3072 \text{ bytes}$$
* **chunk continuation (`m`):**
  * first chunk: transmits image keys + first 3,072 bytes, with `m=1` (or `m=0` if total image $\le 3072$ bytes).
  * intermediate chunks: `\x1b_Gm=1;<payload>\x1b\\`.
  * final chunk: `\x1b_Gm=0;<payload>\x1b\\`.

### iTerm2 inline images protocol (OSC 1337)
* **Escape structure:** Operating System Command (OSC) 1337:
  ```
  \x1b]1337;File=inline=1;width=<cols>;height=<rows>;preserveAspectRatio=1:<base64_payload>\x07
  ```
* **zero-Re-encoding fast path:**
  iTerm2 expects an encoded image file (PNG, JPEG, or GIF) inside the base64 payload.   if the user is viewing an untouched source file, `ashi_view` bypasses decoding entirely:  
  it encodes the original file bytes into base64 and emits the OSC sequence in a single burst.
* **in-memory PNG compression:**
  if the user requested custom background blending or if we are stepping through animated GIF frames, we compress the modified RGBA frame to PNG in memory using `stbi_write_png_to_func` before base64 transmission.

---

# read this after you attempted once to code yourself,  for deep dive or if you are stuck while writing the code.. 

when we move from drawing unicode half-blocks to modern terminal graphics, we stop asking the terminal to print characters and start talking directly to its gpu compositor. both kitty and iterm2 let us pipe raw image data over standard output (stdout) using special escape sequences.  
the catch?  
terminal emulators are built around text streams. if you dump raw binary image bytes straight into stdout, a byte like `0x00 (null)`, `0x07 (bell)`, or `0x1b (escape)` will trick the terminal into thinking your escape sequence ended early or trigger random terminal behaviors.  
to solve this, we will have to make somehow 3-part pipeline: a safe text encoder `base64.c`, a chunked raw-pixel streamer `render_kitty.c`, and an in-memory file streamer `render_iterm.c`.  

1.high_level_architecture
<img width="1582" height="2480" alt="1 high_lvl_arch" src="https://github.com/user-attachments/assets/cb47679b-4130-4d8b-804f-e7007126fe19" />


## 1. turning raw bytes -to-> safe text[base64.c]

computers store images as raw bytes (`8 bits` per byte, values `0 to 255`). terminals expect printable ascii text. base64 bridges this gap by taking every 3 raw bytes (24 bits total) and   slicing them into 4 smaller chunks of 6 bits each 
`3 × 8 = 24`, and` 4 × 6 = 24`.    
since `6 bits` can only hold numbers from `0 to 63` (2⁶ = 64), we can map every 6-bit number to a lookup table `b64_table`  containing 64 completely safe, printable characters `A-Z, a-z, 0-9, +, /`.  

2.base64_bit_slicing 
<img width="3339" height="1052" alt="2 base64_slicing" src="https://github.com/user-attachments/assets/91710415-89e9-4785-a38c-d27a4824d176" />


### our way of writng base64

• calculating the exact buffer size `base64_encoded_size`:
every 3 input bytes become 4 output characters. if the input length isn't divisible by 3, we still need a full 4-character block at the end. in integer math, `((src_len + 2) / 3) * 4 `is the classic trick for rounding up to the nearest multiple of 3 before multiplying by 4.  

• bit-packing three bytes into one integer (triple):
inside the` while (i + 2 < src_len) loop`, we grab 3 consecutive bytes and glue them together into a single 32-bit unsigned integer `uint32_t triple`:
```  
  • src[i] << 16 pushes the first byte into bits 23..16.  
  • src[i + 1] << 8 puts the second byte into bits 15..8.  
  • src[i + 2] sits in the lowest bits 7..0.  
• extracting 6 bits at a time (>> and & 0x3F):  
```
once all 24 bits are lined up inside triple, we slide each 6-bit group down to the bottom right using bit shifts `(>> 18, >> 12, >> 6, >> 0)` and mask it with `0x3F` (which is `00111111` in binary). masking with `0x3F` wipes out any upper bits so we get a clean index between `0` and `63` to look up inside `b64_table`.    
• handling leftovers and padding `=`:  
if the byte count leaves 1 or 2 leftover bytes at the end `if (i < src_len)`, we pack whatever is left into triple and fill the missing slots `with = characters` so the decoder knows the final block was short. finally, we slap `\0` at the end so C string functions like `fprintf` know where the string stops.  

### is this standard practice?

yes. bit-shifting into a 24-bit accumulator with a 64-byte lookup table is the textbook implementation defined [in rfc 4648 the base16, base32, and base64 data encodings spec](https://datatracker.ietf.org/doc/html/rfc4648). you will see this exact same bitwise loop inside openssl, curl, and the linux kernel.

## 2. background alpha blending [`apply_background` in both renderers]

before sending pixels to the terminal, check out `apply_background()` in both `render_kitty.c` and `render_iterm.c`.

### why we need this?
we did these same things in earlier phases ...

pngs and gifs often have transparent or semi-transparent pixels `(where the alpha channel a is less than 255)`.  
if a user passes `--bg checker` or a `solid color` flag, we have to blend the image's foreground color with our chosen background color before transmitting it.

### mathamatically [without slow floats]

#### fast skip `if (a == 255)`:
if a pixel is 100% opaque, we copy r, g, b straight across and skip the math.  
#### procedural checkerboard `((x >> 4) ^ (y >> 4)) & 1`:
shifting x and y right by 4 divides the pixel coordinates by  16 (2⁴ = 16).   
`xoring (^) `those two values and checking the last bit `& 1` alternates between 0 and 1 every 16 pixels, giving us a crisp `16x16` dark gray `(0x38 and 0x24)` transparency grid without storing a separate pattern image in memory.  

#### fixed-point alpha compositing: 
instead of converting pixels to slow floating-point numbers (0.0 to 1.0), we do pure only integer math:  
`(src * a + bg * (255 - a) + 127) / 255`  
adding 127 before dividing by 255 performs proper integer rounding to the nearest whole number instead of truncating downward.  



### 3. kitty graphics protocol [`renderkitty.c`]
kitty's protocol is designed for speed: it lets us stream raw uncompressed rgba pixels straight from ram to the terminal's gpu without spending cpu time compressing them into a png first.  
 
flow in ashi_render_kitty  

##### 1. fitting to the terminal grid: 
we query the terminal size `terminal_get_size` and call `ashi_calc_fit_dimensions` with an `aspect ratio factor of 2.0` (because terminal character cells are roughly twice as tall as they are wide).  
##### 2. why we slice into 3,072-byte chunks (KITTY_RAW_CHUNK_SIZE):  
kitty enforces a strict rule: no single escape sequence payload can be longer than 4,096 base64 characters. if you send a 50,000-character base64 blob in one shot, kitty will drop it or break.  
working backward from the 3-to-4 base64 ratio:  

$$\frac{4096 \text{ base64 chars}}{4} \times 3 \text{ raw bytes} = 3072 \text{ raw bytes}$$


so in `kitty_send_rgba`, we slice our pixel buffer into `3072-byte` pieces `b64_chunk[4096 + 4]` gives us plenty of room for the `4,096` characters plus the null terminator).  

3.kitty_chunking_flow 
<img width="3726" height="1809" alt="3 kitty chunking flow" src="https://github.com/user-attachments/assets/e7f19cd7-c1b7-4730-ac36-14f6f7827b5c" />


##### 3. header keys on the first chunk (is_first):
the very first packet tells kitty what kind of data is coming:  
  • `\x1b_G` — starts an application program command (apc) for kitty graphics.    
  • `a=T` — action = transmit and display right now.  
  • `f=32` — format = 32-bit raw rgba (4 bytes per pixel).  
  • `s=%d`,`v=%d` — source pixel width (s) and height (v).  
  • `c=%d`,`r=%d` — how many terminal columns (c) and rows (r) the image should occupy.  
  • `m=%d` — "more data coming" flag (1 if more chunks follow, 0 if this was the only chunk).  
  • `\x1b\\` — string terminator (st) that closes the escape sequence.  

##### 4. continuation chunks (else):
every subsequent chunk skips repeating the image dimensions and just sends `\x1b_Gm=1;<payload>\x1b\\`, until the very last chunk sets `m=0 `to tell kitty "all pieces have arrived, draw the texture now.

###### here's the officical reffrence i think you should look at.. [kitty graphics protocol doc](https://sw.kovidgoyal.net/kitty/graphics-protocol/)

## 4. iterm2 inline imaged protocol [`render_iterm.c`]

iterm2 (and terminals that adopt its protocol, like wezterm) works differently from kitty. it does not accept raw uncompressed rgba bytes, it expects a complete, valid image file (like a png, jpeg, or gif) encoded inside a single base64 string.  

2 paths in ashi_render_iterm  

##### 1. the zero-copy fast path (BG_NONE):
if the user didn't ask for a custom background color `cfg->bg.mode == BG_NONE` and we still have the original file bytes sitting in `img->raw_data`, we don't even touch the decoded pixels. we `base64-encode` the original file bytes straight off disk/ram and send them immediately. zero re-encoding overhead.
##### 2. the in-memory png path (encode_frame_to_png):
if we modified the pixels (for example, blending a checkerboard background or extracting individual frames from an animated gif), those pixels only exist as raw rgba arrays in memory. we have to compress them back into a valid png before iterm2 will accept them.  

4.iterm_memory_callback 
<img width="8388" height="933" alt="5 iterm_mem_callback" src="https://github.com/user-attachments/assets/fdbed16c-89af-476b-84a5-77170bc73c26" />



###### how MemBuffer and mem_write_callback work

stb_image_write.h normally writes pngs to a file on disk,   which would be slow and messy for a terminal viewer. instead, we use `stbi_write_png_to_func`, passing it our own `mem_write_callback` and a `MemBuffer struct`:  

• every time `stb_image_write` compresses a piece of the png, it calls `mem_write_callback` with a `pointer` to those bytes.  
• our callback checks `if buf->size + size` exceeds `buf->cap`  . if it does, it doubles the capacity (`new_cap *= 2`, starting at 16 kb) using `realloc` and copies the new bytes onto the end with `memcpy`.  
• `geometric capacity doubling (* 2)` is an industry-standard dynamic array pattern (it's literally how C++ std::vector and Rust Vec grow) because it keeps memory reallocations fast `O(1) amortized time` instead of calling realloc on every tiny write.  

##### 3. emitting osc 1337 `iterm_send_encoded`:

once we have the base64 string, we wrap it in an operating system command (osc) 1337 sequence:  
`\x1b]1337;File=inline=1;width=%d;height=%d;preserveAspectRatio=1:%s\x07\n`
notice the `\x07` (ascii bell character) at the end—that's the mandatory terminator that tells iterm2 the base64 payload is finished.

you can read more from [iterm docs](https://iterm2.com/documentation-images.html)

### 5. smooth gif animation loop[shared by both protocols] 
both our ptotocols share the same animation rendering loop

##### pre-processing upfront: 
before the animation starts, we pre-blend all frames (in kitty) or pre-encode all frames to pngs in memory (in iterm2). doing heavy compression inside the playback loop would cause visible stutter.
##### overwriting the previous frame (terminal_cursor_up(rows + 1)): 
after frame 0 prints (plus its trailing \n), the cursor sits rows + 1 lines below the top of the image. before drawing frame 1, we move the cursor back up by rows + 1 lines so the new frame draws directly on top of the old one instead of scrolling down the terminal endlessly.

##### drift-free frame pacing (t_start and elapsed): 
encoding and printing a frame takes a few milliseconds. if a gif frame is supposed to stay on screen for `100ms` and sending it took `12ms`, `sleeping` for a` full 100ms` would make the animation run slow. by measuring `elapsed = terminal_time_ms() - t_start `and sleeping only for `delay - elapsed`, our frame rate stays locked to the gif's true timing.



---

## 3. technologies / libraries / APIs used

### we write ourselves
* `include/base64.h` & `src/base64.c`: Lookup-table base64 encoder.
* `include/render_kitty.h` & `src/render_kitty.c`: Kitty chunked streaming engine.
* `include/render_iterm.h` & `src/render_iterm.c`: iTerm2 OSC 1337 engine with memory write callback.

### libraries / dependencies
* **`vendor/stb_image_write.h`**: In-memory PNG compression (`stbi_write_png_to_func`).

### platform / runtime APIs
* Standard C memory and I/O: `malloc()`, `realloc()`, `free()`, `fprintf()`, `fwrite()`.

### external APIs / services
* *None.*

---

## 4. reference Project Map

inspect these files in the reference project:

* `include/base64.h` & `src/base64.c`:
  * Look at `b64_table` and the 3-byte bit-shifting loop (`triple = (b0 << 16) | (b1 << 8) | b2`).
* `src/render_kitty.c`:
  * Look at `KITTY_RAW_CHUNK_SIZE` (3072).
  * Look at `kitty_send_rgba`: chunking loop with `m=1` and `m=0`.
* `src/render_iterm.c`:
  * Look at `MemBuffer` and `mem_write_callback` passed to `stbi_write_png_to_func`.
  * Look at `iterm_send_encoded` emitting OSC 1337.
* `vendor/stb_image_write.h`: Included and compiled via `src/stb_impl.c`.

---

## 5. major tasks

### Task 1 — Implement the Base64 Encoder
- 1.1 In `src/base64.c`, implement `base64_encoded_size()`.
- 1.2 Implement `base64_encode()` using bitwise shifts and standard alphabet table.
- 1.3 Add padding characters (`=`) for remainder blocks.

### Task 2 — Implement Kitty Graphics Protocol
- 2.1 In `src/render_kitty.c`, calculate fit dimensions (`ashi_calc_fit_dimensions`).
- 2.2 Implement chunked streaming: slice raw 32-bit RGBA into 3,072-byte blocks.
- 2.3 Format first chunk with `a=T,f=32,s=W,v=H,c=C,r=R` and subsequent chunks with `m=1`/`m=0`.
- 2.4 Handle animation frame loop with `terminal_cursor_up()`.

### Task 3 — Implement iTerm2 Inline Images Protocol
- 3.1 In `src/render_iterm.c`, implement `encode_frame_to_png()` using `stbi_write_png_to_func` with dynamic memory callback.
- 3.2 Implement fast-path raw file streaming when no background blending is requested.
- 3.3 Emit OSC 1337 escape sequence with base64 data.

### Task 4 — Connect Protocol Flags & Verify
- 4.1 Update `src/main.c` to accept `-p kitty` and `-p iterm`.
- 4.2 Test rendering in compatible terminal emulators.

---

## 6. detailed implementation tasks

### Task 1 — Implement the Base64 Encoder
#### 1.1
In `include/base64.h`, declare:
```c
size_t base64_encoded_size(size_t src_len);
size_t base64_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_cap);
```
#### 1.2
In `src/base64.c`, define `static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";`.
#### 1.3
In `base64_encode`:
Loop while $i + 2 < \text{src len}$
:
Combine 3 bytes into a 24-bit [uint_32-t taske 32 bit] integer:
`triple = (src[i] << 16) | (src[i+1] << 8) | src[i+2];`
Extract 4 six-bit chunks:
* `dst[o++] = b64_table[(triple >> 18) & 0x3F];`
* `dst[o++] = b64_table[(triple >> 12) & 0x3F];`
* `dst[o++] = b64_table[(triple >> 6)  & 0x3F];`
* `dst[o++] = b64_table[triple & 0x3F];`
Advance $i$ by 3.

#### 1.4
Handle remaining 1 or 2 bytes by packing into `triple`, indexing table, and appending `=` padding characters. Null-terminate `dst`.

### Task 2 — Implement Kitty Graphics Protocol
#### 2.1
Define `KITTY_RAW_CHUNK_SIZE 3072`.
#### 2.2
In `kitty_send_rgba(rgba, w, h, cols, rows)`:
Maintain `offset = 0` up to `total_bytes = w * h * 4`.
In a `while (offset < total_bytes)` loop:
* `chunk_raw = min(total_bytes - offset, 3072)`.
* Call `base64_encode(rgba + offset, chunk_raw, b64_chunk, sizeof(b64_chunk))`.
* `is_last = (offset + chunk_raw >= total_bytes)`.
* If first chunk:
  ```c
  fprintf(stdout, "\x1b_Ga=T,f=32,s=%d,v=%d,c=%d,r=%d,m=%d;%s\x1b\\",
          w, h, cols, rows, is_last ? 0 : 1, b64_chunk);
  ```
* If subsequent chunk:
  ```c
  fprintf(stdout, "\x1b_Gm=%d;%s\x1b\\", is_last ? 0 : 1, b64_chunk);
  ```
* Advance `offset += chunk_raw`.
#### 2.3
Append `\n` and `fflush(stdout)`.

### Task 3 — Implement iTerm2 Inline Images Protocol
#### 3.1
In `src/stb_impl.c`, add `#define STB_IMAGE_WRITE_IMPLEMENTATION` before `#include "stb_image_write.h"`.
#### 3.2
In `src/render_iterm.c`, define a dynamic buffer structure:
```c
typedef struct {
    uint8_t *data;
    size_t size;
    size_t cap;
} MemBuffer;
```
Implement `mem_write_callback(void *context, void *data, int size)` to realloc `buf->data` as needed and `memcpy`.
#### 3.3
Implement `encode_frame_to_png()`:
Call `stbi_write_png_to_func(mem_write_callback, &buf, w, h, 4, pixels, 0)`.
Return `buf.data` and set `*out_len = buf.size`.
#### 3.4
Implement `iterm_send_encoded(data, len, cols, rows)`:
* Calculate required base64 size with `base64_encoded_size(len)`.
* Allocate heap buffer `b64 = malloc(b64_len + 1)`.
* Encode and print:
  ```c
  fprintf(stdout, "\x1b]1337;File=inline=1;width=%d;height=%d;preserveAspectRatio=1:%s\x07\n",
          cols, rows, b64);
  ```
* Free `b64`.

---

## 7. Pseudocode

### Base64 Encoding (src/base64.c)

```
function base64_encode(src, src_len, dst, dst_capacity):
    table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    i = 0, o = 0

    while i + 2 < src_len:
        triple = (src[i] << 16) OR (src[i+1] << 8) OR src[i+2]
        dst[o+0] = table[(triple >> 18) AND 0x3F]
        dst[o+1] = table[(triple >> 12) AND 0x3F]
        dst[o+2] = table[(triple >> 6)  AND 0x3F]
        dst[o+3] = table[triple AND 0x3F]
        i += 3
        o += 4

    if i < src_len:
        rem = src_len - i
        triple = src[i] << 16
        if rem == 2:
            triple = triple OR (src[i+1] << 8)

        dst[o+0] = table[(triple >> 18) AND 0x3F]
        dst[o+1] = table[(triple >> 12) AND 0x3F]
        if rem == 2:
            dst[o+2] = table[(triple >> 6) AND 0x3F]
            dst[o+3] = '='
        else:
            dst[o+2] = '='
            dst[o+3] = '='
        o += 4

    dst[o] = '\0'
    return o
```

### Kitty Chunked Streamer (src/render_kitty.c)

```
function kitty_send_rgba(pixels, width, height, target_cols, target_rows):
    total_bytes = width * height * 4
    offset = 0
    is_first = true

    while offset < total_bytes:
        bytes_to_send = min(total_bytes - offset, 3072)
        chunk_b64 = base64_encode(pixels + offset, bytes_to_send)
        offset += bytes_to_send
        is_last = (offset >= total_bytes)
        more_flag = is_last ? 0 : 1

        if is_first:
            print "\x1b_Ga=T,f=32,s=" + width + ",v=" + height +
                  ",c=" + target_cols + ",r=" + target_rows +
                  ",m=" + more_flag + ";" + chunk_b64 + "\x1b\\"
            is_first = false
        else:
            print "\x1b_Gm=" + more_flag + ";" + chunk_b64 + "\x1b\\"

    print "\n"
    flush_stdout()
```

### iTerm2 Streamer (src/render_iterm.c)

```
function ashi_render_iterm(image, config):
    calculate_fit_dimensions(image.width, image.height, &cols, &rows)

    if config.bg.mode == BG_NONE and image.raw_data is not null:
        // Fast-path: send original file directly
        b64_string = base64_encode(image.raw_data, image.raw_size)
        print "\x1b]1337;File=inline=1;width=" + cols + ";height=" + rows +
              ";preserveAspectRatio=1:" + b64_string + "\x07\n"
        return SUCCESS

    // Custom background / dynamic frame path
    png_data = compress_frame_to_png(image.frames[0].pixels, image.width, image.height)
    b64_string = base64_encode(png_data)
    print "\x1b]1337;File=inline=1;width=" + cols + ";height=" + rows +
          ";preserveAspectRatio=1:" + b64_string + "\x07\n"
    free(png_data)
    return SUCCESS
```

---

## 8. Build Order

1. Create `include/base64.h` and implement `src/base64.c`.
2. Write a quick unit test in `src/main.c` encoding small test strings (`"hello" -> "aGVsbG8="`).
3. Add `vendor/stb_image_write.h` and configure `src/stb_impl.c`.
4. Implement `include/render_kitty.h` and `src/render_kitty.c`.
5. Implement `include/render_iterm.h` and `src/render_iterm.c`.
6. Update `Makefile` to compile all new `.c` files.
7. Compile and test with `-p kitty` and `-p iterm`.

---

## 9. Testing & Verification

### Test 1 — Base64 Verification
Verify `base64_encode` with known RFC 4648 test vectors:
* `"foo"` $\rightarrow$ `"Zm9v"`
* `"foobar"` $\rightarrow$ `"Zm9vYmFy"`

### Test 2 — Kitty Protocol Output Test
Run:
```bash
./ashi_view -p kitty -w 40 samples/star_transparent.png
```
**Expected Observation:**
* If running inside Kitty or Ghostty: The image renders at full retina display resolution!
* If running in standard terminal: You will see APC characters `\x1b_G...` printed on screen (proving the escape sequence was emitted).

### Test 3 — iTerm2 Protocol Output Test
Run:
```bash
./ashi_view -p iterm -w 40 samples/star_transparent.png
```
**Expected Observation:**
* If running inside WezTerm or iTerm2: Image renders inline at full display resolution.
* If running in standard terminal: Terminal ignores OSC 1337 or prints nothing.

---

proceed to next and the last guide..
