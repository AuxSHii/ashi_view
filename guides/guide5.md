# Phase 5 — The Animated GIF Playback Engine

## 1. Goal

in this part of our guide, you will transform your image viewer from a static display tool into an **Animated GIF Playback Engine**.

You will add multi-frame GIF decoding via `stbi_load_gif_from_memory`, implement high-precision monotonic frame timing, introduce a **pre-resampling frame cache** for zero-latency frame swaps, and achieve **flicker-free in-place animation** using terminal cursor repositioning (`\x1b[<rows>A\r`). You will also add non-blocking keyboard controls so users can exit animations by pressing `q` or `ESC`.

we have image as binary[raw] and in our struct
if you are confused how they look like in memory , see this
<img width="1756" height="2736" alt="0 1_pixel_in_mem" src="https://github.com/user-attachments/assets/a9938e86-c614-456c-85dd-c1302cb7ebc8" />

before we implement anything the next thing i want you to brush up is our image decoding pipeline
this is how final image pipeline will look:
<img width="2057" height="7128" alt="0 decoding_pipeliene" src="https://github.com/user-attachments/assets/d44820cf-f53d-4294-b0f8-3850fa1d26b7" />

---

## 2. what you should understand before coding

### multi-frame GIF architecture
an animated GIF consists of:
* a logical screen header defining overall canvas width and height.
* a sequence of $N$ graphic frames.
* each frame has its own Graphic Control Extension specifying:
  * frame delay in hundredths of a second ($10\text{ ms}$ units).
  * disposal method (e.g., leave as is, restore to background, restore to previous).
  * transparent color index.

`stb_image` abstracts away the disposal arithmetic and outputs a contiguous 32-bit RGBA pixel block of size:
$$\text{Total Buffer Size} = W \times H \times N \times 4 \text{ bytes}$$
along with a dynamically allocated array of integers containing the frame delays.

### the browser delay quirk ($\le 10\text{ ms}$)
in the 1990s, early GIF encoders wrote `delay = 0` or `delay = 1` ($10\text{ ms}$), expecting browsers to play them at a reasonable speed (~10 FPS). If played literally at $0\text{ ms}$, the CPU spins at 100% and the animation blurs into an unwatchable strobe.
* **Standard Web/Browser Rule:** if `delay <= 10`, clamp the delay to **$100\text{ ms}$** (10 FPS).


### Pre-Resampling vs. Real-Time Resampling
if an animated GIF has 60 frames and is originally 800×600 pixels:
* downsampling a single frame takes ~1–2 milliseconds.
* if you downsample inside the playback loop, you consume CPU and introduce frame stutter.
* **Pre-resampling strategy:** Allocate an array of $N$ downsampled frame buffers *before* the playback loop begins:
$$\text{scaled frames}[f] = \text{resample}(\text{source frame}[f], \text{cols}, \text{rows} \times 2)$$

<img width="1284" height="2240" alt="2 pre-sampling_cache" src="https://github.com/user-attachments/assets/6cced7c2-f8ac-40e2-9b66-0861bee825d1" />



during playback, swapping frames is a simple memory pointer change taking $< 0.1\text{ ms}$.


### flicker-Free cursor repositioning
the naive way to animate in a terminal is:
1. Clear screen (`\x1b[2J\x1b[H`).
2. Render frame.
3. Sleep.

*why this fails:* Clearing the screen introduces severe black-flash flickering.
* **the Solution:** On the first frame, render normally. On every subsequent frame, move the cursor up by exactly the number of lines the image occupies:
  $$\mathtt{\backslash x1b[<rows>A\backslash r}$$
  * `\x1b[<N>A`: Move cursor UP by $N$ rows.
  * `\r`: Carriage return (move cursor to column 0).
  the terminal redraws the new pixels directly over the old pixels in-place with zero flicker.

<img width="1222" height="213" alt="3 flicker_free_cursor" src="https://github.com/user-attachments/assets/ec713d3b-1176-47db-b411-3ab5d94cc6d6" />



so we are essentialy re-writing the next frame over the last frame , we are not gonna clear the screen that takes time..


### monotonic clocks vs. wall-clock time
* wall-clock functions (such as `gettimeofday()` or `time()`) can jump forward or backward if the system clock syncs with an NTP server.
* **Monotonic Clocks** (`clock_gettime(CLOCK_MONOTONIC)` on POSIX, `QueryPerformanceCounter` on Windows) are guaranteed to never jump or run backward. They provide microsecond-accurate time intervals for calculating how long to sleep between frames.


---

# read this after you code,for deep-dive or if you get stuck while writing code

---

## for TASK 1

i know this section is focused on implementing animated gif support, so it might feel like we are jumping straight into the deep end. before we get lost in gif delays and frame loops, i want to walk you through the entire image decoding pipeline inside src/image.c from the ground up. if anything in the earlier phases felt unclear  
read this after you code for deep-dive or if you get stuck while writing code  

i know this section is focused on implementing animated gif support, so it might feel like we are jumping straight into the deep end. before we get lost in gif delays and frame loops, i want to walk you through the entire image decoding pipeline inside src/image.c from the ground up. if anything in the earlier phases felt fuzzy—like why we read files into memory first, what magic bytes are, or how stb_image manages pixel memory—this will clear it all up.  

#### 1. how computers actually see files (and why extensions lie)

when you look at a file named cat.gif or photo.png, your operating system does not actually care about the .gif or .png at the end. file extensions are just friendly labels for humans and desktop file managers. if you rename photo.png to photo.gif, the bytes inside the file do not change at all....  
to a c program, every file on your hard drive is just a long, flat list of numbers (bytes, each ranging from 0 to 255).  

#### what are magic signatures?

because file names cannot be trusted, file format creators put a secret handshake right at the very beginning of the file usually the first 4 to 8 bytes. in systems programming, we call these magic numbers or magic signatures.  
• png files always start with the 8 bytes: 89 50 4E 47 0D 0A 1A 0A (fun thing: notice 50 4E 47 in ascii spells PNG). ;)   
• jpeg files always start with FF D8 FF.  
• gif files always start with 6 ascii characters: either "GIF87a" (the original 1987 standard) or "GIF89a" (the 1989 update that added animation and transparency).  
in `src/image.c`, look at how we detect a gif:
```c
bool is_gif = (size >= 6 && (memcmp(raw, "GIF87a", 6) == 0 || memcmp(raw, "GIF89a", 6) == 0));
```

two things are happening in this line:  
1. bounds checking (size >= 6): before we inspect the first 6 bytes of raw, we verify the file actually has at least 6 bytes. if someone passes a 2-byte corrupt file and we call memcmp for 6 bytes, we would read past the end of our allocated heap buffer (undefined behavior / potential crash).
2. memcmp instead of strcmp: strcmp expects a null-terminated string ('\0'), whereas raw binary file buffers are not strings. memcmp compares an exact number of raw bytes (6 bytes) without caring about null terminators.

ALSO real-world tools like the linux file command, web browsers (chrome/firefox), and security scanners ignore file extensions and inspect these exact magic bytes (often called "mime sniffing") to decide which decoder to run.  

#### 2. why we load the entire file into raw first

in ashi_image_load, we do not pass the file path directly to stb_image. instead, we do the fopen -> fseek -> ftell -> malloc -> fread dance to pull the entire compressed file into a heap buffer called raw (and save it on img->raw_data).
why bother doing this all manually?  
1. peeking at headers: we need to read the first 6 bytes (GIF87a/GIF89a) before choosing which stb_image function to call.
2. keeping compressed bytes for later: in phase 6 (when we add native terminal protocols like iterm2 and kitty), some terminals let us send the original compressed png/gif bytes directly without decoding them into raw pixels. keeping img->raw_data around saves us from opening the file twice.
3. faster memory decoding: reading from ram (stbi_load_from_memory / stbi_load_gif_from_memory) avoids repeated small disk reads during decoding.

#### 3. understanding stb_image data types and pixel layout

stb_image.h is one of the most famous single-header c libraries in the world, used in countless indie games, engines, and tools.

##### what is stbi_uc?

when you see stbi_uc *gif_pixels or stbi_uc *pixels, do not let the custom type name intimidate you. inside stb_image.h, it is simply defined as:  
```c
typedef unsigned char stbi_uc;
```

an `unsigned char` is guaranteed to be an `8-bit integer` holding values from `0 to 255` (the exact same thing as uint8_t from `<stdint.h>`).  

##### how pixels sit in memory (rgba)

<img width="1698" height="1387" alt="img data viiusal-phase 5" src="https://github.com/user-attachments/assets/a087fa17-6a66-4aa6-a7b9-1c5659552270" />

when we pass 4 as the last argument (desired_channels = 4) to stbi_load_from_memory or stbi_load_gif_from_memory, we are telling stb_image: "no matter whether the original image was grayscale, rgb, or palette-indexed, convert every single pixel into 4 bytes: red, green, blue, and alpha."  

<img width="3578" height="1233" alt="img data actual storage" src="https://github.com/user-attachments/assets/545afb98-55aa-4d99-a860-6c91a232eff6" />

so a single pixel takes 4 bytes:  
• byte 0: red (0 to 255)  
• byte 1: green (0 to 255)  
• byte 2: blue (0 to 255)  
• byte 3: alpha / transparency (0 = completely invisible, 255 = completely solid)  
look at ashi_check_has_alpha at the top of src/image.c:  
```c
if (pixels[i * 4 + 3] < 255)
```

for pixel number i, we jump forward i * 4 bytes to reach the start of that pixel, and then add + 3 to inspect its 4th byte (the alpha channel).

#### 4. how stbi_load_gif_from_memory lays out an animation

a static image is just a 2d grid of `width * height` pixels. an animated gif is a 3d stack of frames essentially frames number of 2d images stacked one after another in one giant contiguous chunk of heap memory.
```
gif_pixels in memory:
[ frame 0 (w * h * 4 bytes) ][ frame 1 (w * h * 4 bytes) ][ frame 2 (w * h * 4 bytes) ] ...
^                            ^                            ^
gif_pixels + 0               gif_pixels + 1*frame_bytes   gif_pixels + 2*frame_bytes
```

##### pointer arithmetic in our slicing loop

this is how our slicing work:
<img width="6258" height="3852" alt="1 simpler_slicing+more" src="https://github.com/user-attachments/assets/578dde7f-5ea2-442c-9a73-fe3966ea702c" />


inside `src/image.c`, we want each `img->frames[i]` to own its own independent pixel buffer so every frame looks identical in structure to a static image. look at how we slice that giant block:
```c
size_t frame_byte_size = (size_t)w * (size_t)h * 4;
// ...
memcpy(img->frames[i].pixels, gif_pixels + (size_t)i * frame_byte_size, frame_byte_size);
```

because gif_pixels is a pointer to a 1-byte type `(stbi_uc *)`, adding `(size_t)i * frame_byte_size` moves the memory address forward by that exact number of bytes, landing cleanly on the first pixel of frame i.  

(note: notice how we cast w, h, and i to size_t before multiplying. if you load a huge high-res gif, multiplying plain 32-bit int variables could overflow past 2 billion bytes; casting to size_t prevents integer overflow.)  
  
#### what stb_image handles for us

under the hood, gif files do not always store full frames. to save file size, frame 2 might only contain a tiny 10x10 patch of changed pixels to draw on top of frame 1 (called `disposal methods` and `frame sub-rects`). stbi_load_gif_from_memory does all the messy compositing math for us so every slice in gif_pixels is a complete, full-canvas w * h image ready to render.  

### 5. the two allocators rule (and a subtle catch in ashi_image_free)  

when managing memory in c, every malloc must be paired with a free. however, stb_image uses its own wrapper (STBI_MALLOC / STBI_FREE), which might be customized in larger projects.   therefore, the our golden rule is:    
• memory allocated by `stb_image` must be freed with `stbi_image_free()`.  
• memory allocated by our own `malloc()` must be freed with standard `free()`.  
look at how `src/image.c` handles both paths:
1. animated gif path: gif_pixels came from stb_image, so we copy each frame into our own malloc() buffers, and then immediately call `stbi_image_free(gif_pixels)` and `free(delays)` before returning. now `img->frames[i].pixels` belongs 100% to our malloc.
2. static image path: pixels came from `stbi_load_from_memory`, and we assign `img->frames[0].pixels = pixels` directly without copying it. so that pointer still belongs to `stb_image`.

#### watch out for a subtle edge case!

take a close look at `ashi_image_free()` at the bottom of `src/image.c`:
```c
if (!img->is_animated && img->frame_count == 1) {
    stbi_image_free(img->frames[i].pixels);
} else {
    free(img->frames[i].pixels);
}
```

#### what happens if a user loads a 1-frame gif?
1. is_gif is true, and stbi_load_gif_from_memory succeeds with frames == 1.
2. inside the if (is_gif) block, img->is_animated = (frames > 1) evaluates to false, while img->frame_count is 1.
3. in that block, img->frames[0].pixels was allocated with our standard malloc(frame_byte_size).
4. later, ashi_image_free sees !img->is_animated && img->frame_count == 1 and calls stbi_image_free() on a buffer that was allocated with malloc()!
now .... on most desktop systems `stbi_image_free` defaults to `free()`, so it won't crash right away, but if someone ever defines `custom STBI_MALLOC allocators`, that mismatch causes heap corruption. a cleaner design is to either add a `bool owned_by_stbi` flag on` ashi_frame_t`, or simply copy/allocate uniformly so every pixels buffer is freed the exact same way.

### 6. defensive cleanup: rollback on partial malloc failure

look at lines `127–138` in `src/image.c`:
```c
if (!img->frames[i].pixels) {
    /* Handle partial allocation failure */
    for (int j = 0; j < i; ++j) {
        free(img->frames[j].pixels);            
    }
    free(img->frames);
    stbi_image_free(gif_pixels);
    free(delays);
    free(raw);
    return -1;
}
```

imagine loading a massive 200-frame gif when your system is almost out of ram. frames 0 through 149 succeed, and frame 150 returns NULL. if you just returned -1 immediately, the 150 buffers you already allocated would be leaked forever. walking backward from j = 0 to i - 1 and freeing every prior frame before returning is classic, production-grade c resource management (often called the `rollback pattern`).  

hence we are safe!!!

### 7. not free-ing raw data at the end of gif decoding :

why we didntnot free'ed raw data of image from memory here... because:  
1. phase 6 needs the original compressed bytes (kitty & iterm2 protocols)

remember in earlier phases we decoded the image into raw { r, g, b, a } pixels so we can draw unicode half-blocks (▀) in any terminal?
• in upcoming phases i am planning , we'll add native high-resolution graphics protocols for modern terminals like iterm2 and kitty.      
• iterm2's inline image protocol (and animated gif pass-through) does not want decoded { r, g, b, a } pixels—it literally expects you to base64-encode the original raw .png, .jpg, or .gif file bytes (img->raw_data of length img->raw_size) and send them straight to the terminal!    
• by saving img->raw_data = raw inside ashi_image_load(), we can immediately grab img->raw_data without having to re-open or re-read the file from disk.  

2. notice when we do vs. don't free raw

look closely at the two outcomes of ashi_image_load():
• if loading fails (return -1): we call free(raw) immediately before returning so nothing leaks on an error.  
• if loading succeeds (return 0): ownership of raw transfers into the ashi_image_t struct (img->raw_data = raw). then, when the whole viewer is done and cleans up at the end of main(), ashi_image_free(img) runs:  
```c
if (img->raw_data) {
    free(img->raw_data);
    img->raw_data = NULL;
}
```
#### ALSO in industrial context:
keeping the original compressed source buffer attached to a decoded asset struct is standard practice in media players and game engines ,it lets different rendering backends (software fallback vs. hardware/native protocol) pick whichever representation they need without touching the filesystem twice.

---

## for TASK 2

so we have till now , successfully decoded the image or gif we will now be making the terminal to playback gif etc in real time..

### 0. problems to solve
if you render an image once, you just print characters and exit. when you animate, 3 new systems problems appear out of nowhere:  

1. how do we measure exact elapsed time so frame 1 stays on screen for 100ms and not 40ms or 300ms?
2. how do we pause the cpu between frames without burning 100% of a processor core?
3. how do we listen for the user pressing q or ESC without freezing the animation while waiting for the enter key?

we implement the solutions in `terminal.c`
<img width="2525" height="9273" alt="entire terminal flow" src="https://github.com/user-attachments/assets/0131ffaa-b82a-42bc-a604-41f8dda75d57" />


### 1. monotonic clocks(terminal_time_ms) -
our system's clock cant be trusted in a regular fashion.  

inside your computer, there are actually 2 different kinds of clock:  
1. the wall clock (CLOCK_REALTIME / gettimeofday): this is the clock shown in the corner of your screen (e.g., 3:05 PM, Nov 13).  
its job is to match human calendar time. because human time needs corrections, your operating system silently adjusts this clock all the time..  
syncing with internet ntp servers, applying daylight saving time, or letting the user manually change the time in settings.  


2. the monotonic clock (CLOCK_MONOTONIC / QueryPerformanceCounter):   
think of this as a hardware stopwatch that starts ticking upward from 0 the exact millisecond your motherboard powers on.  
it has no idea what year or time of day it is.  
its only guarantee is that every tick moves forward at a rock-solid, constant physical rate and never jumps backward or forward.  

<img width="2216" height="1236" alt="5 wall_clock_vs_monotonic_clock" src="https://github.com/user-attachments/assets/0c0c1db7-5d65-49e1-93d3-383d0b672bdd" />



#### why we need it in gif player?

suppose frame 0 starts at t_start = 1000ms on a wall clock.  halfway through rendering that frame, windows or linux syncs with an internet time server and nudges your clock backward by 50ms.  
• now t_now = 960ms.  
• we compute elapsed = t_now - t_start = 960 - 1000 = -40ms.
• because uint64_t is an unsigned integer, -40 underflows into 18,446,744,073,709,551,576 milliseconds (roughly 584 million years!). your gif freezes forever.  

<img width="1640" height="479" alt="4  monotonic_frame_pacing" src="https://github.com/user-attachments/assets/7eee2579-28bb-45e0-bc1b-654bfc657ef9" />



### how we read it?
#### on posix [linux/mac OS]
```c
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);

```
1.  clock_gettime fills a struct timespec with two fields: tv_sec (whole seconds) and tv_nsec (nanoseconds, where 1,000,000 nanoseconds = 1 millisecond).
2.  we convert seconds to milliseconds (* 1000ULL) and nanoseconds to milliseconds (/ 1000000ULL), using ULL (unsigned long long, 64-bit) so the math here never overflows 32-bit integers.


so we will know in ms , how much time has passed ..

#### on windows
```c
LARGE_INTEGER count, freq;
QueryPerformanceCounter(&count);
QueryPerformanceFrequency(&freq);
return (uint64_t)((count.QuadPart * 1000ULL) / freq.QuadPart);

```
1. windows exposes the cpu's high-resolution hardware timer via QueryPerformanceCounter, which returns raw hardware ticks (count.QuadPart).  
2. because different motherboards tick at different speeds (e.g., 10 mhz vs 3.5 ghz), we ask QueryPerformanceFrequency how many ticks occur per second (freq.QuadPart), multiply ticks by 1000ULL first (to preserve precision before integer division!), and divide by freq.  

ALSO in context of INDUSTRIAL applications  
every game engine (unreal, unity, godot), browser (performance.now() in javascript), and video player (mpv, vlc) uses CLOCK_MONOTONIC on unix and QueryPerformanceCounter on windows for frame pacing and delta-time physics.  


## 2. sleeping without spiking the cpu
a beginner often writes a delay like this:
```c
// ❌ terrible idea: "busy-waiting"
while (terminal_time_ms() - start < 100) { }

```

that empty while loop forces your cpu core to run at 100% utilization, heating up your laptop and draining battery just to wait 100 milliseconds.  
instead, we call nanosleep() on posix and Sleep() on windows. these are kernel system calls: our program tells the operating system scheduler, "put my thread to sleep and give my cpu core to other apps; wake me up in ms milliseconds." while sleeping, our program uses 0% cpu.  

#### POSIX
```c
ts.tv_sec = ms / 1000;
ts.tv_nsec = (long)(ms % 1000) * 1000000L;
nanosleep(&ts, NULL);

```
nanosleep requires tv_nsec to be strictly less than 1,000,000,000 (1 second).  
if someone passes ms = 1500 (1.5 seconds), 1500 / 1000 puts 1 into tv_sec, and the modulo (1500 % 1000) * 1000000L puts the remaining 500,000,000 nanoseconds into tv_nsec.

## 3.non-blocking keyboard input [exit keys]

the problem with getchar() and scanf() is:  when we call them  
program stops dead in its tracks and waits until the user types a character and hits enter. if we called getchar() inside our gif loop, frame 0 would draw, and the animation would freeze completely until you pressed a key!

we need a non-blocking way to input keys: "has the user pressed a key right this microsecond? if yes, grab it; if no, return false immediately in 0ms so we can draw the next frame."  


### implementation 

#### on windows(_kbbhit + _getch):


_kbhit() checks the windows console input buffer without waiting.   
if a keystroke is sitting in the queue, it returns non-zero, and _getch() pulls that single character immediately without waiting for enter and without echoing the letter q onto your artwork.

#### on posix (poll with timeout = 0):

```c
struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 };
if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) { ... }
}
```
poll() asks the linux/macos kernel to inspect file descriptor 0 (STDIN_FILENO). the third argument 0 is the timeout in milliseconds—passing 0 tells the kernel "do not wait at all; check right now and return immediately."  
if POLLIN (data ready to read) is set, we read 1 raw byte via read().

(note for phase 5/6 polish: on posix terminals in default "canonical" line-buffered mode, the terminal driver holds keypresses until enter is pressed unless ICANON and ECHO are disabled via tcsetattr in terminal_init—something to keep in mind if q requires enter on linux/mac!)

## 4. terminal life cycle , signals & ansi cursor control

1. terminal_init():
  • on windows, turns on ENABLE_VIRTUAL_TERMINAL_PROCESSING (so cmd.exe understands \x1b[... ansi color and cursor codes) and CP_UTF8 (so half-block unicode characters ▀ render instead of garbled symbols).  
  • on posix, saves your original terminal settings (tcgetattr) so we can restore them cleanly later.  
  • registers sig_handler for SIGINT (ctrl+c) and SIGTERM. if the user hits ctrl+c mid-animation while the cursor is hidden (\x1b[?25l), sig_handler runs terminal_restore() so your terminal cursor isn't left invisible forever!  
2. terminal_cursor_up(int rows):
  • prints \x1b[<rows>A\r. \x1b[<rows>A moves the cursor straight up rows lines, and \r (carriage return) snaps it all the way back to column 0 (top-left corner of our image) so the next frame paints directly over the previous one with zero screen clearing.  


---

## 3. technologies / libraries / APIs Used

### we write ourselves
* `src/image.c`: GIF magic header detection and multi-frame decoding.
* `include/terminal.h` & `src/terminal.c`: Monotonic timers (`terminal_time_ms`, `terminal_sleep_ms`) and non-blocking key checking (`terminal_check_key`).
* `src/render_halfblock.c`: The animated playback loop and frame pacing controller.

### Libraries / dependencies
* **`vendor/stb_image.h`**: `stbi_load_gif_from_memory()`.

### Platform / runtime APIs
* **POSIX:**
  * `clock_gettime(CLOCK_MONOTONIC, &ts)`: High-resolution monotonic timer.
  * `nanosleep(&ts, NULL)`: High-resolution sleep.
  * `poll(&pfd, 1, 0)`: Non-blocking check on `STDIN_FILENO`.
* **Windows:**
  * `QueryPerformanceCounter` / `QueryPerformanceFrequency`: High-precision timer.
  * `Sleep(ms)`: Millisecond sleep.
  * `_kbhit()` & `_getch()` (from `<conio.h>`): Non-blocking key hit detection.

### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `src/image.c`:
  * Look for the GIF magic detection: `memcmp(raw, "GIF87a", 6) == 0 || memcmp(raw, "GIF89a", 6) == 0`.
  * Look for the `stbi_load_gif_from_memory()` call and the frame allocation loop.
* `src/terminal.c`:
  * Look at `terminal_time_ms()` and `terminal_sleep_ms()`.
  * Look at `terminal_check_key()` for Windows (`_kbhit`) and POSIX (`poll`).
* `src/render_halfblock.c`:
  * Look at the animated loop inside `ashi_render_halfblock()`: the cursor jump `terminal_cursor_up(rows)`, frame pacing, and keyboard break logic.

---

## 5. Major Tasks

### Task 1 — Implement Multi-Frame GIF Decoding
- 1.1 In `src/image.c`, check for `GIF87a` / `GIF89a` magic signatures in the raw file buffer.
- 1.2 Call `stbi_load_gif_from_memory()` to extract frame count, delays, and RGBA pixel planes.
- 1.3 Populate `ashi_image_t` with an array of `ashi_frame_t` structs, clamping delays $\le 10\text{ ms}$ to $100\text{ ms}$.

### Task 2 — Implement Monotonic Timers & Non-Blocking Keyboard Input
- 2.1 In `src/terminal.c`, implement `terminal_time_ms()` using high-resolution OS monotonic timers.
- 2.2 Implement `terminal_sleep_ms()` for millisecond sleep.
- 2.3 Implement `terminal_check_key()` to poll standard input without blocking execution.

### Task 3 — Implement the Flicker-Free Playback Loop
- 3.1 In `src/render_halfblock.c`, pre-allocate and resample all $N$ frames before starting playback.
- 3.2 Implement the loop: render frame, calculate elapsed time, sleep for remaining delay, check for user quit key.
- 3.3 Emit `terminal_cursor_up(rows)` between frames to achieve in-place frame overwriting.
- 3.4 Ensure cursor visibility and colors are restored on exit.

### Task 4 — Add CLI Animation Controls
- 4.1 Support `-l` / `--loop` (infinite loop).
- 4.2 Support `-1` / `--once` (play once and exit).
- 4.3 Support `-n` / `--loops <count>` (play specific count).
- 4.4 Support `-s` / `--speed <multiplier>` (e.g. `1.5` for 1.5× speed).

---

## 6. Detailed Implementation Tasks

### Task 1 — Implement Multi-Frame GIF Decoding
#### 1.1
In `src/image.c`, inspect the first 6 bytes of `img->raw_data`.
Check if `memcmp(raw, "GIF87a", 6) == 0` or `memcmp(raw, "GIF89a", 6) == 0`.
#### 1.2
If true, declare `int *delays = NULL`, `int w, h, frames, comp`.
Call:
```c
stbi_uc *gif_pixels = stbi_load_gif_from_memory(raw, (int)size, &delays, &w, &h, &frames, &comp, 4);
```
#### 1.3
If `gif_pixels && frames > 0`:
* Set `img->width = w`, `img->height = h`, `img->frame_count = frames`, `img->is_animated = (frames > 1)`.
* Allocate `img->frames = malloc(frames * sizeof(ashi_frame_t))`.
* For each frame $i = 0 \dots \text{frames}-1$:
  * Allocate `img->frames[i].pixels = malloc(w * h * 4)`.
  * `memcpy` slice from `gif_pixels + i * (w * h * 4)`.
  * Read `d = delays[i]`. If $d \le 10$, set $d = 100$. Store in `img->frames[i].delay_ms`.
* Free `gif_pixels` with `stbi_image_free()` and `free(delays)`.
#### 1.4
In `ashi_image_free()`, update the cleanup loop to free each frame's pixels using standard `free()` when `is_animated` is true.

### Task 2 — Implement Monotonic Timers & Non-Blocking Keyboard Input
#### 2.1
In `src/terminal.c`:
* Under Windows: use `QueryPerformanceCounter` divided by `QueryPerformanceFrequency` multiplied by 1000.
* Under POSIX: use `clock_gettime(CLOCK_MONOTONIC, &ts)`. Return $(tv\_sec \times 1000) + (tv\_nsec / 1000000)$.
#### 2.2
In `terminal_check_key(char *out_key)`:
* Under Windows: if `_kbhit()` returns true, read char with `_getch()` and return `true`.
* Under POSIX: call `poll()` on `STDIN_FILENO` with timeout 0. If `POLLIN` is set, read 1 byte and return `true`. Otherwise return `false`.

### Task 3 — Implement the Flicker-Free Playback Loop
#### 3.1
In `src/render_halfblock.c`:
Check if `img->is_animated && img->frame_count > 1`.
Allocate array `uint8_t **scaled_frames = malloc(img->frame_count * sizeof(uint8_t *))`.
For each frame $f$:
Allocate `cols * (rows * 2) * 4` bytes.
Call `ashi_resample_rgba(img->frames[f].pixels, img->width, img->height, scaled_frames[f], cols, rows * 2)`.
#### 3.2
Hide the cursor: `terminal_hide_cursor()`.
Set up loop counter:
```
while not g_terminal_interrupted and (infinite or current_loop < max_loops):
    for f from 0 to frame_count - 1:
        record t_start = terminal_time_ms()

        if current_loop > 0 or f > 0:
            terminal_cursor_up(rows)

        render_frame_halfblock(scaled_frames[f], cols, rows, bg, outbuf)

        // Calculate sleep duration
        target_delay = img->frames[f].delay_ms / speed
        elapsed = terminal_time_ms() - t_start
        if elapsed < target_delay:
            terminal_sleep_ms(target_delay - elapsed)

        // Check for exit key
        if terminal_check_key(&key):
            if key == 'q' or key == 'Q' or key == 27:
                break out of animation
```
#### 3.3
After exiting the loop:
Show the cursor (`terminal_show_cursor()`), reset color (`terminal_reset_color()`), free all `scaled_frames`, and exit cleanly.

---

## 7. Pseudocode

### Multi-Frame GIF Decoding (src/image.c)

```
function load_gif_frames(raw_data, file_size, image_struct):
    delays = null
    w = 0, h = 0, frame_count = 0, comp = 0

    all_pixels = stbi_load_gif_from_memory(raw_data, file_size, &delays, &w, &h, &frame_count, &comp, 4)
    if all_pixels is null:
        return false

    image_struct.width = w
    image_struct.height = h
    image_struct.frame_count = frame_count
    image_struct.is_animated = (frame_count > 1)
    image_struct.frames = allocate_memory(frame_count * size_of(ashi_frame_t))

    frame_bytes = w * h * 4
    for i from 0 to frame_count - 1:
        image_struct.frames[i].width = w
        image_struct.frames[i].height = h
        image_struct.frames[i].pixels = allocate_memory(frame_bytes)
        copy_memory(image_struct.frames[i].pixels, all_pixels + (i * frame_bytes), frame_bytes)

        raw_delay = delays[i]
        // Netscape/browser minimum delay clamp
        image_struct.frames[i].delay_ms = (raw_delay <= 10) ? 100 : raw_delay

    stbi_image_free(all_pixels)
    free_memory(delays)
    return true
```

### Animation Pacing Loop (src/render_halfblock.c)

```
function run_animation_loop(scaled_frames, frame_delays, frame_count, cols, rows, loop_count, speed):
    terminal_hide_cursor()

    is_infinite = (loop_count <= 0)
    current_loop = 0

    while not interrupted and (is_infinite or current_loop < loop_count):
        for f from 0 to frame_count - 1:
            frame_start_time = terminal_time_ms()

            // Overwrite previous frame in-place
            if current_loop > 0 or f > 0:
                print "\x1b[" + rows + "A\r"

            draw_halfblock_frame(scaled_frames[f], cols, rows)

            // Pacing
            effective_delay = frame_delays[f] / speed
            time_spent = terminal_time_ms() - frame_start_time
            if time_spent < effective_delay:
                sleep_milliseconds(effective_delay - time_spent)

            // Interactive controls
            if check_keyboard_pressed(&key):
                if key == 'q' or key == 'Q' or key == ESCAPE:
                    interrupted = true
                    break

        current_loop += 1

    terminal_show_cursor()
    terminal_reset_color()
```

---

## 8. Build Order

1. Implement GIF magic detection and `stbi_load_gif_from_memory()` in `src/image.c`.
2. Update `ashi_image_free()` to properly clean up multi-frame arrays.
3. Test `-i` on a GIF file (`./ashi_view -i samples/spinner.gif`) to verify that frame count and animated status are reported accurately.
4. Implement `terminal_time_ms()`, `terminal_sleep_ms()`, and `terminal_check_key()` in `src/terminal.c`.
5. Implement the pre-resampling cache and the `terminal_cursor_up()` playback loop in `src/render_halfblock.c`.
6. Add CLI options (`-l`, `-1`, `-n`, `-s`) to `src/main.c`.
7. Compile and test playback.

---

## 9. Testing & Verification

### Test 1 — Inspect Animated GIF Metadata
Run:
```bash
./ashi_view -i samples/spinner.gif
```
**Expected Observation:**
```
File:        samples/spinner.gif
Dimensions:  64 x 64 px
Channels:    RGBA (8-bit)
Has Alpha:   yes
Animated:    yes
Frames:      12
```

### Test 2 — Single Loop Playback
Run:
```bash
./ashi_view -p halfblock -w 30 -1 samples/spinner.gif
```
**Expected Observation:** The spinner animation plays smoothly through its 12 frames without screen flickering, completes 1 rotation, and returns to your shell prompt cleanly with the cursor visible.

### Test 3 — Interactive Interruption
Run:
```bash
./ashi_view -p halfblock -w 30 -l samples/spinner.gif
```
Press `q` while the GIF is playing.
**Expected Observation:** The animation stops immediately, the cursor reappears, and the terminal is left in its default state.

---

proceed to phase 6.
