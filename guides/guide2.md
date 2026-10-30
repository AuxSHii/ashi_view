# Phase 2 — Image Decoding & Memory Representation

## 1. Goal

In this phase, you will implement the core image loading and memory management subsystem. You will integrate the single-header image decoder `stb_image`, load image files (PNG, JPEG, BMP) from disk into 32-bit RGBA pixel buffers in heap memory, detect whether an image contains translucent/transparent pixels, and build a CLI command that inspects and displays image metadata.

By the end of this phase, your application will be able to read any standard image file, decode its compressed format into raw uncompressed pixels, and safely release that memory when finished.


encoded image -->single header img decoder--> 32bit  rgba pixel buffer[raw uncompressed pixels]

---

## 2. What You Should Understand Before Coding

### Raster Image & Pixel Channels
RASTER IMAGE — A rectangular grid of individual colored points called pixels.
A pixel's color is represented by a combination of channels:
* **RGB (3 channels):** Red, Green, Blue. In 8-bit color, each channel has a value between 0 and 255 (1 byte per channel, totaling 3 bytes per pixel).
* **RGBA (4 channels):** Red, Green, Blue, Alpha. The Alpha channel defines opacity (0 = completely transparent, 255 = completely opaque). Each pixel takes 4 bytes (32 bits).

### Row-Major Memory Layout
In C, a 2D image of width $W$ and height $H$ with 4 channels is stored in a flat 1D array of size:
$$\text{Total Bytes} = W \times H \times 4$$
Pixels are ordered row-by-row (row-major order). The pixel at column $x$ and row $y$ begins at memory offset:
$$\text{Byte Offset} = (y \times W + x) \times 4$$
* Byte `offset + 0`: Red
* Byte `offset + 1`: Green
* Byte `offset + 2`: Blue
* Byte `offset + 3`: Alpha

### Single-Header C Libraries
`stb_image.h` is a public-domain, header-only C library created by Sean Barrett.
* When included normally (`#include "stb_image.h"`), it acts as a normal header providing declarations.
* When preceded by `#define STB_IMAGE_IMPLEMENTATION` in exactly **one** `.c` file, it compiles the full implementation.
* If defined in more than one `.c` file, the linker will produce duplicate symbol errors. Therefore, we dedicate a single translation unit (`src/stb_impl.c`) exclusively to compiling third-party implementations.


also:
  component that processes a compressed latent representation (from an encoder) to reconstruct or predict a single type of output map or image



### Memory Ownership & Heap Lifecycles
Compressed image files (e.g. 50 KB PNG) expand into large uncompressed memory buffers (e.g. 1920×1080×4 bytes = ~8.3 MB) when decoded.
* Every heap buffer allocated by `malloc()` or `stbi_load()` must have a clear owner.
* A destructor function (`ashi_image_free`) must guarantee that all allocated buffers (raw file bytes, frame arrays, pixel planes) are freed without memory leaks.
---
## 3. Technologies / Libraries / APIs Used
### We write ourselves
* `include/image.h`: Data structures (`ashi_frame_t`, `ashi_image_t`) and function prototypes for image loading and freeing.
* `src/image.c`: File reading, decompression invocation, alpha scanning, and heap deallocation.
* `src/stb_impl.c`: Dedicated translation unit compiling `stb_image`.

### Libraries / dependencies
* **`vendor/stb_image.h`**: Decodes PNG, JPEG, BMP, TGA, PSD, GIF, and HDR images from memory buffers without external dependencies (like `libpng` or `libjpeg`).
  * Official Repository: [nothings/stb on GitHub](https://github.com/nothings/stb)
  * Functions used: `stbi_load_from_memory()`, `stbi_image_free()`, `stbi_failure_reason()`.

### Platform / runtime APIs
* Standard C file I/O: `fopen()`, `fseek()`, `ftell()`, `fread()`, `fclose()`.
* Standard memory management: `malloc()`, `free()`, `memset()`.

### read about them

ere is a breakdown of what each function does, written just like memset.

📂 1. fopen() — Open a File

The fopen function is used to create a link between your C program and a file on the disk. It prepares the file for reading, writing, or appending.
• Syntax: FILE *fopen(const char *filename, const char *mode);
• What it does: It takes the file's name and a "mode" (like "r" for read, "w" for write, or "rb" for read-binary).
• Return Value: It returns a pointer to a FILE structure. If the file cannot be opened (e.g., it doesn't exist), it returns NULL.

🗺️ 2. fseek() — Move the File Pointer

The fseek function is used to jump to a specific location inside a file. Think of it like moving the playback cursor on a video timeline.
• Syntax: int fseek(FILE *stream, long offset, int whence);
• What it does: It moves the cursor by a certain number of bytes (offset) starting from a position (whence).
    • SEEK_SET: Start moving from the beginning of the file.
    • SEEK_CUR: Start moving from the current cursor position.
    • SEEK_END: Start moving from the end of the file.
• Return Value: Returns 0 if successful, or non-zero if it fails.

📏 3. ftell() — Get the Current Position

The ftell function tells you exactly where the file cursor is currently sitting, measured in bytes from the start of the file.
• Syntax: long ftell(FILE *stream);
• What it does: It acts like a ruler to tell you how far into the file you have read or jumped.
• Return Value: Returns the current byte position.
• 💡 Pro-Tip (Find File Size): A classic C trick to find a file's total size is to use fseek to jump to the very end (SEEK_END), and then call ftell(). It will return the total size of the file in bytes!
thats what we are gonna do and use in a short while..

📥 4. fread() — Read Data Block

The fread function is used to read a specific block of raw bytes from a file into a memory buffer (like an array or a struct). It is highly efficient for binary data.
• Syntax: size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
• What it does: It reads a number of items (count), where each item is a specific number of bytes (size), from the file stream and copies them into the memory buffer (ptr).
• Return Value: Returns the number of items successfully read. If this number is less than count, you've either reached the end of the file or encountered an error.

🔒 5. fclose() — Close the File

The fclose function breaks the link between your program and the file. It saves any remaining changes to the disk and frees up system memory.
• Syntax: int fclose(FILE *stream);
• What it does: It cleans up after you are done. If you forget to call it, you risk corrupting the file or running into a "memory/file leak" where the operating system locks the file.
• Return Value: Returns 0 if successfully closed, or EOF if an error occurs.



 6. memset
The memset function in C is a standard library function used to fill a block of memory with a specific value. It is defined in the <string.h> header file and operates byte by byte, making it highly efficient for initializing or clearing memory structures like arrays and structs.

7. rewind(file) : used to reset the file position cursor back to the very beginning of a file.



### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `vendor/stb_image.h`: The vendored image decoder library.
* `src/stb_impl.c`: Defines `STB_IMAGE_IMPLEMENTATION` and includes the vendor header.
* `include/image.h`: Declares `ashi_image_load()`, `ashi_image_free()`, and `ashi_check_has_alpha()`.
* `include/ashi.h`: Defines `ashi_frame_t` (holding width, height, pixel pointer, delay) and `ashi_image_t` (holding frames array, count, raw bytes).
* `src/image.c`: Implementation of image loading and lifecycle management.

---

## 5. Major Tasks

### Task 1 — Integrate stb_image
- 1.1 Download or vendor `stb_image.h` into `vendor/`.
- 1.2 Create `src/stb_impl.c` with compiler diagnostic pragmas to suppress third-party compiler warnings.
- 1.3 Update `Makefile` to compile `src/stb_impl.c` with include path `-Ivendor`.

### Task 2 — Define Image Data Structures
- 2.1 Define `ashi_frame_t` in `include/ashi.h` to store pixel dimensions and RGBA buffer pointer.
- 2.2 Define `ashi_image_t` to store global width, height, channel count, frame count, frames pointer, and raw file bytes.
- 2.3 Declare loading, freeing, and alpha inspection functions in `include/image.h`.

### Task 3 — Implement Image Loading & Alpha Scanning
- 3.1 Implement whole-file binary reading (`fopen`, `fseek`, `fread`) into a heap buffer.
- 3.2 Invoke `stbi_load_from_memory` requesting 4 channels (`STBI_rgb_alpha`).
- 3.3 Implement `ashi_check_has_alpha()` by iterating over all pixels and checking if `alpha < 255`.
- 3.4 Implement `ashi_image_free()` to safely free all buffers and reset pointers.

### Task 4 — Implement Metadata Inspection CLI (`-i` / `--info`)
- 4.1 Update `src/main.c` to accept a file path argument and optional `-i` flag.
- 4.2 If `-i` is passed, print resolution, channels, alpha presence, file size, and exit cleanly.

---

## 6. Detailed Implementation Tasks

### Task 1 — Integrate stb_image
#### 1.1
Place `stb_image.h` inside a `vendor/` directory.
#### 1.2
Create `src/stb_impl.c`. Add:
```c
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
```
Wrap this in `#pragma GCC diagnostic ignored` blocks if using GCC/Clang to avoid warnings from third-party code under `-Wextra`.
#### 1.3
Add `src/stb_impl.c` to `SRC` in your `Makefile`. Add `-Ivendor` to `CPPFLAGS`.

### Task 2 — Define Image Data Structures
#### 2.1
In `include/ashi.h`, define `ashi_frame_t`:
* `int width`, `int height`
* `uint8_t *pixels` (points to raw 32-bit RGBA bytes)
* `int delay_ms` (set to 0 for static images)
#### 2.2
In `include/ashi.h`, define `ashi_image_t`:
* `int width`, `int height`, `int channels`
* `int frame_count` (1 for static images)
* `ashi_frame_t *frames` (array of frames)
* `bool is_animated`, `bool has_alpha`
* `uint8_t *raw_data`, `size_t raw_size` (stores original compressed file bytes)
#### 2.3
In `include/image.h`, declare prototypes:
* `int ashi_image_load(const char *path, ashi_image_t *img);`
* `void ashi_image_free(ashi_image_t *img);`
* `bool ashi_check_has_alpha(const uint8_t *pixels, int pixel_count);`

### Task 3 — Implement Image Loading & Alpha Scanning
#### 3.1
In `src/image.c`, open the file in binary mode (`"rb"`). Use `fseek(f, 0, SEEK_END)` and `ftell(f)` to get the file size. Allocate `malloc(size)` and read all bytes with `fread`.
#### 3.2
Pass the raw buffer to `stbi_load_from_memory(raw, size, &w, &h, &comp, 4)`.
* If it returns NULL, print the failure reason from `stbi_failure_reason()`, free the raw buffer, and return an error code.
#### 3.3
Allocate `img->frames = malloc(sizeof(ashi_frame_t))`. Assign `w`, `h`, and the returned `pixels` pointer to `img->frames[0]`.
#### 3.4
Implement `ashi_check_has_alpha`:
Loop $i$ from 0 to $\text{pixel\_count} - 1$. Inspect `pixels[i * 4 + 3]`. If less than 255, return `true`. If loop finishes, return `false`.
#### 3.5
Implement `ashi_image_free`:
Check if `img->frames` is non-NULL. For single static frames, free pixels with `stbi_image_free()`. Free `frames` and `raw_data`. Reset all fields to zero.

### Task 4 — Implement Metadata Inspection CLI
#### 4.1
In `src/main.c`, parse command-line arguments. If argv contains `-i` or `--info`, set a boolean flag `show_info = true`.
#### 4.2
Load the image via `ashi_image_load()`. If `show_info` is true, print formatted lines:
* File path
* Dimensions (e.g. `256 x 256 px`)
* Channels (`RGBA (8-bit)`)
* Has Alpha (`yes` or `no`)
* File size in bytes
#### 4.3
Call `ashi_image_free()` and exit with code 0.

---

## 7. Pseudocode

### Image Loading (src/image.c)

```
function ashi_image_load(file_path, image_struct):
    clear image_struct to zero

    file_handle = open_file(file_path, "rb")
    if file_handle is null:
        print error "cannot open file"
        return ERROR_FILE_NOT_FOUND

    seek to end of file_handle
    file_size = tell_position(file_handle)
    rewind file_handle

    if file_size <= 0:
        close file_handle
        return ERROR_EMPTY_FILE

    raw_buffer = allocate_memory(file_size)
    bytes_read = read_file(file_handle, raw_buffer, file_size)
    close file_handle

    store raw_buffer and file_size in image_struct

    // Decompress image into 32-bit RGBA
    pixels = stbi_load_from_memory(raw_buffer, file_size, &w, &h, &channels, 4)
    if pixels is null:
        print error stbi_failure_reason()
        free_memory(raw_buffer)
        return ERROR_DECODE_FAILED

    image_struct.width = w
    image_struct.height = h
    image_struct.channels = 4
    image_struct.frame_count = 1
    image_struct.is_animated = false

    image_struct.frames = allocate_memory(size_of(ashi_frame_t))
    image_struct.frames[0].width = w
    image_struct.frames[0].height = h
    image_struct.frames[0].pixels = pixels
    image_struct.frames[0].delay_ms = 0

    image_struct.has_alpha = ashi_check_has_alpha(pixels, w * h)
    return SUCCESS
```

### Alpha Channel Detection (src/image.c)

```
function ashi_check_has_alpha(pixels, pixel_count):
    for i from 0 to pixel_count - 1:
        alpha_value = pixels[i * 4 + 3]
        if alpha_value < 255:
            return true
    return false
```

### Memory Cleanup (src/image.c)

```
function ashi_image_free(image_struct):
    if image_struct is null:
        return

    if image_struct.frames is not null:
        for each frame in image_struct.frames:
            if frame.pixels is not null:
                stbi_image_free(frame.pixels)
        free_memory(image_struct.frames)

    if image_struct.raw_data is not null:
        free_memory(image_struct.raw_data)

    clear image_struct to zero
```

---

## 8. Build Order

1. Add `vendor/stb_image.h` and create `src/stb_impl.c`.
2. Update `Makefile` to include `src/stb_impl.c` and compile.
3. Define structs in `include/ashi.h` and function prototypes in `include/image.h`.
4. Implement `ashi_check_has_alpha()` in `src/image.c`.
5. Implement whole-file reader and `stbi_load_from_memory()` integration in `src/image.c`.
6. Implement `ashi_image_free()`.
7. Update `src/main.c` with the `-i` / `--info` inspection logic.
8. Compile and test with sample PNG and JPEG images.

---

## 9. Testing & Verification

### Test 1 — Compilation
Run:
```bash
make
```
**Expected Observation:** Binary compiles cleanly. `stb_impl.o` is generated.

### Test 2 — Inspect Opaque JPEG Image
Run:
```bash
./ashi_view -i samples/star_transparent.png
```
**Expected Observation:** Output prints:
```
File:        samples/star_transparent.png
Dimensions:  256 x 256 px
Channels:    RGBA (8-bit)
Has Alpha:   yes
Animated:    no
File Size:   5785 bytes
```

### Test 3 — Invalid File Handling
Run:
```bash
./ashi_view -i non_existent_file.png
```
**Expected Observation:** Program prints a clear error message (e.g. `cannot open file 'non_existent_file.png'`), does not segfault, and returns exit code 1.

---

## 10. Common Beginner Mistakes

* **Mixing up `free()` and `stbi_image_free()`:**
  * *Symptom:* Crashes or heap corruption warnings when freeing image memory.
  * *Why:* Memory allocated by `stbi_load` should be freed by `stbi_image_free()`.
  * *Fix:* Ensure `stbi_image_free(frame.pixels)` is used for buffers returned by `stb_image`.
* **Assuming 3 Channels Instead of 4:**
  * *Symptom:* Image appears tilted, sheared, or colors are wrong.
  * *Why:* If you do not force 4 channels (`req_comp = 4`), a standard JPEG returns 3 channels (RGB) while a PNG returns 4 channels (RGBA).
  * *Fix:* Always pass `4` for `req_comp` in `stbi_load_from_memory` so all internal buffers consistently use 32-bit RGBA.
* **Reading Files in Text Mode on Windows:**
  * *Symptom:* Decompression fails unexpectedly on Windows.
  * *Why:* In text mode (`"r"`), Windows translates `\r\n` to `\n` and treats byte `0x1A` as EOF.
  * *Fix:* Always use `"rb"` (read binary).

---

## 11. Reference Comparison

Compare your work against the reference implementation:
* Check `src/image.c`: Notice how `ashi_image_load()` checks file magic headers to distinguish GIFs from other image formats.
* Check error handling: If `fread` reads fewer bytes than `file_size`, notice that `raw_data` is freed immediately before returning.

---

## 12. Build-from-Memory Checkpoint

Without looking at the reference code:
1. Write the mathematical formula to calculate the byte index of the blue channel of a pixel at coordinate `(x, y)` in a row-major 32-bit RGBA buffer of width `W`.
2. Why is `#define STB_IMAGE_IMPLEMENTATION` placed in `src/stb_impl.c` instead of `include/image.h`?
3. How do you distinguish whether an image contains translucent pixels?

---

## 13. Phase Completion Checklist

- [ ] `vendor/stb_image.h` integrated via `src/stb_impl.c`.
- [ ] `ashi_frame_t` and `ashi_image_t` defined in `include/ashi.h`.
- [ ] Binary file reading and `stbi_load_from_memory` implemented.
- [ ] 32-bit RGBA decoding verified.
- [ ] Alpha transparency scanning implemented.
- [ ] `ashi_image_free()` deallocates all memory without leaks.
- [ ] CLI `-i` / `--info` flag prints accurate metadata.
- [ ] Build-from-memory checkpoint completed.
- [ ] Git commit created.

---

## 14. Git Commit

### What to commit:
* `vendor/stb_image.h`
* `src/stb_impl.c`
* `include/image.h`
* `src/image.c`
* Updated `src/main.c` and `Makefile`

### Commit message suggestion:
```
feat: implement image decoding and memory lifecycle subsystem
```

After this commit, proceed to [Phase 3 — Sizing Geometry & Area-Averaging Box Resampling](file:///c:/Users/Alter-Book/Downloads/loki/c_img_mapper/guides/guide3.md).
