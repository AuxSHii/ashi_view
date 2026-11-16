# Phase 7 - CLI Interface, Auto-Detection & Final Polish

## 1. Goal

finally we are here....

in this **final** phase, you will integrate all components into a polished, production-grade CLI application. You will be making:
1. an environment **protocol auto-detection** (`protocol.h` and `protocol.c`).
2. a unified **rendering dispatcher** (`ashi_render`).
3. a user-friendly **command-line parser** supporting short flags (`-p`, `-w`, `-h`, `-b`, `-l`, `-s`, `-i`), long options, and hex background parsing (`#RRGGBB`).
4. build targets for CMake and installation (`make install`).

by the end of this phase, our project will be completely self-contained, fully functional, and ready to install on any Linux, macOS, or windows system

---

## 2. what you should understand before coding

### terminal environment heuristics
when a user launches a terminal, the terminal emulator injects identifying environment variables into the shell process. By inspecting these variables via `getenv()`, `ashi_view` can detect graphical capabilities without requiring the user to specify flags:
* **Kitty / Ghostty:**
  * `KITTY_WINDOW_ID` or `KITTY_PID` is defined.
  * `TERM` contains `"kitty"` or `"ghostty"`.
* **iTerm2 / WezTerm / Mintty / Warp:**
  * `TERM_PROGRAM` equals `"iTerm.app"`, `"WezTerm"`, `"mintty"`, or `"WarpTerminal"`.
  * `LC_TERMINAL` equals `"iTerm2"`.
* **Standard Terminal Fallback:**
  * if none of the above signatures match (e.g. inside standard `tmux`, `xterm`, `alacritty`, `gnome-terminal`, or Windows Terminal), default safely to the universal Unicode half-block rasterizer (`PROTO_HALFBLOCK`).

<img width="1538" height="2986" alt="protol_auto-detection" src="https://github.com/user-attachments/assets/345568df-6f4b-493e-842c-85c2ea691bf6" />


<img width="640" height="360" alt="robust" src="https://github.com/user-attachments/assets/d6221454-2a6b-4ee2-bb86-f252e1c37847" />


	 
### robust CLI option parsing
a production CLI program must:
1. handle both flags (`-c`, `--clear`, `-l`, `--loop`) and flags with values (`-w 80`, `--width 80`).
2. reject unrecognized arguments with helpful error messages.
3. validate user input (e.g., negative widths, invalid protocol names, malformed hex color codes).
4. provide comprehensive `--help` and `--version` documentation.

---

## 3. technologies / libraries / APIs Used

### We write ourselves
* `include/protocol.h` & `src/protocol.c`: Protocol detection heuristic and render dispatching.
* `src/main.c`: Full CLI argument parser, option validation, and program entry point.
* `CMakeLists.txt`: Optional CMake build script.

### Libraries / dependencies
* *None.* (Pure standard C).

### Platform / runtime APIs
* Standard C environment lookup: `getenv()`.
* Standard C string manipulation: `strcmp()`, `strstr()`, `sscanf()`, `atoi()`, `atof()`.

### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `include/protocol.h`: Defines `ashi_protocol_t` enum (`PROTO_AUTO`, `PROTO_KITTY`, `PROTO_ITERM`, `PROTO_HALFBLOCK`) and declares `ashi_detect_protocol()` and `ashi_render()`.
* `src/protocol.c`: Implements the `getenv` checks and dispatches to `ashi_render_kitty`, `ashi_render_iterm`, or `ashi_render_halfblock`.
* `src/main.c`: Complete CLI option parsing loop, `print_usage()`, `parse_bg()`, and clean exit handling.
* `CMakeLists.txt`: Demonstrates portable CMake target definition.

---

## 5. major tasks

### task 1 — implement protocol auto-detection & dispatcher
- 1.1 In `include/protocol.h`, declare `ashi_detect_protocol()`, `ashi_protocol_name()`, and `ashi_render()`.
- 1.2 In `src/protocol.c`, implement `getenv()` heuristic checks.
- 1.3 Implement `ashi_render()` to resolve `PROTO_AUTO` and invoke the selected renderer.

### Task 2 — Implement Command-Line Option Parser
- 2.1 In `src/main.c`, implement `print_usage()` with usage examples.
- 2.2 Implement `parse_bg()` supporting `checker`, `black`, `white`, `none`, and `#RRGGBB` hex colors.
- 2.3 Implement the argument parsing loop across `argv`.
- 2.4 Handle `-h`, `--help`, `-v`, `--version`, and `-i`, `--info`.

### Task 3 — Add Build Targets & Installation Rules
- 3.1 Update `Makefile` with `install` target (`install -m 755 ashi_view $(DESTDIR)$(PREFIX)/bin/`).
- 3.2 Create `CMakeLists.txt` for cross-platform CMake builds.

### Task 4 — Perform Full End-to-End Test Suite
- 4.1 Test static PNG with default and custom backgrounds.
- 4.2 Test animated GIF with custom speed and loop limits.
- 4.3 Test forced protocol flags (`-p halfblock`, `-p kitty`, `-p iterm`).
- 4.4 Test error conditions (missing file, invalid flag).

---

## 6. Detailed Implementation Tasks

### Task 1 — Implement Protocol Auto-Detection & Dispatcher
#### 1.1
In `src/protocol.c`:
Check Kitty:
```c
if (getenv("KITTY_WINDOW_ID") || getenv("KITTY_PID")) return PROTO_KITTY;
const char *term = getenv("TERM");
if (term && (strstr(term, "kitty") || strstr(term, "ghostty"))) return PROTO_KITTY;
```
#### 1.2
Check iTerm2 / WezTerm / Mintty / Warp:
```c
const char *prog = getenv("TERM_PROGRAM");
if (prog && (strcmp(prog, "iTerm.app") == 0 || strcmp(prog, "WezTerm") == 0 ||
             strcmp(prog, "mintty") == 0 || strcmp(prog, "WarpTerminal") == 0)) {
    return PROTO_ITERM;
}
const char *lc = getenv("LC_TERMINAL");
if (lc && strcmp(lc, "iTerm2") == 0) return PROTO_ITERM;
```
#### 1.3
Default to `PROTO_HALFBLOCK`.
#### 1.4
In `ashi_render(img, cfg)`:
If `cfg->protocol == PROTO_AUTO`, assign `active = ashi_detect_protocol()`.
Switch on `active` and call the matching render function.

### Task 2 — Implement Command-Line Option Parser
#### 2.1
In `src/main.c`, define helper `parse_bg(arg, &cfg->bg)`:
* If `arg == "checker"`, set mode `BG_CHECKER`.
* If `arg == "none"`, set mode `BG_NONE`.
* If `arg == "black"`, set mode `BG_SOLID` and RGB `0, 0, 0`.
* If `arg == "white"`, set mode `BG_SOLID` and RGB `255, 255, 255`.
* If `arg[0] == '#'`: use `sscanf(arg, "#%02x%02x%02x", &r, &g, &b)`. If 3 values matched, store RGB and set mode `BG_SOLID`.
#### 2.2
In `main()`:
Initialize `ashi_config_t cfg` with defaults:
* `protocol = PROTO_AUTO`
* `target_cols = 0`, `target_rows = 0`
* `bg = { BG_CHECKER, 0, 0, 0 }`
* `speed = 1.0f`
#### 2.3
Iterate $i$ from 1 to $\text{argc}-1$:
* Parse `-p` / `--protocol`: check `auto`, `kitty`, `iterm`, `halfblock`.
* Parse `-w` / `--width` and `-h` / `--height` using `atoi()`.
* Parse `-b` / `--bg` with `parse_bg()`.
* Parse `-s` / `--speed` using `atof()`.
* Parse `-l`, `-1`, `-n`.
* Parse `-c` (clear screen), `-i` (metadata), `-v` (version).
* If argument starts with `'-'`, print unknown flag error.
* Otherwise, store as `cfg.file_path`.
#### 2.4
If no file path was given, print usage and exit with code 1.

### Task 3 — Add Build Targets & Installation Rules
#### 3.1
In `Makefile`, add:
```makefile
PREFIX ?= /usr/local
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/
```
#### 3.2
Create `CMakeLists.txt` specifying `project(ashi_view LANGUAGES C)`, C99 standard, and linking `m` on non-MSVC compilers.

---

## 7. Pseudocode

### Protocol Detection (src/protocol.c)

```
function ashi_detect_protocol():
    if getenv("KITTY_WINDOW_ID") is not null or getenv("KITTY_PID") is not null:
        return PROTO_KITTY

    term = getenv("TERM")
    if term contains "kitty" or term contains "ghostty":
        return PROTO_KITTY

    term_program = getenv("TERM_PROGRAM")
    if term_program in ["iTerm.app", "WezTerm", "mintty", "WarpTerminal"]:
        return PROTO_ITERM

    if getenv("LC_TERMINAL") == "iTerm2":
        return PROTO_ITERM

    return PROTO_HALFBLOCK
```

### CLI Option Parser (src/main.c)

```
function main(argc, argv):
    if argc < 2:
        print_usage(argv[0])
        return 1

    config = default_config()

    for i from 1 to argc - 1:
        arg = argv[i]

        if arg in ["--help"]:
            print_usage(argv[0])
            return 0
        else if arg in ["-v", "--version"]:
            print "ashi_view version " + VERSION
            return 0
        else if arg in ["-i", "--info"]:
            config.show_info = true
        else if arg in ["-p", "--protocol"]:
            i += 1
            config.protocol = parse_protocol(argv[i])
        else if arg in ["-w", "--width"]:
            i += 1
            config.target_cols = string_to_int(argv[i])
        else if arg in ["-h", "--height"]:
            i += 1
            config.target_rows = string_to_int(argv[i])
        else if arg in ["-b", "--bg"]:
            i += 1
            parse_background(argv[i], &config.bg)
        else if arg in ["-s", "--speed"]:
            i += 1
            config.speed = string_to_float(argv[i])
        else if arg in ["-l", "--loop"]:
            config.loop_count = -1
        else if arg in ["-1", "--once"]:
            config.loop_count = 1
        else if arg in ["-n", "--loops"]:
            i += 1
            config.loop_count = string_to_int(argv[i])
        else if arg starts with "-":
            print error "unknown flag"
            return 1
        else:
            config.file_path = arg

    if config.file_path is null:
        print error "no input file"
        return 1

    terminal_init()
    image = load_image(config.file_path)

    if config.show_info:
        print_image_metadata(image)
        free_image(image)
        terminal_restore()
        return 0

    exit_code = ashi_render(image, config)

    free_image(image)
    terminal_restore()
    return exit_code
```

---

## 8. Build Order

1. Implement `include/protocol.h` and `src/protocol.c`.
2. Implement CLI argument parsing in `src/main.c`.
3. Add `src/protocol.c` to `Makefile` and compile.
4. Test all CLI flags with sample images.
5. Create `CMakeLists.txt` and verify installation targets.
6. Run full verification suite.

---

## 9. Testing & Verification

### Test 1 — Help Documentation
Run:
```bash
./ashi_view --help
```
**Expected Observation:** Formatted CLI reference is printed, showing protocols, dimensions, background options, and animation flags.

### Test 2 — Hex Color Background Parsing
Run:
```bash
./ashi_view -p halfblock -w 30 -b "#ff5500" samples/star_transparent.png
```
**Expected Observation:** Transparent cutout regions render with vibrant orange background (`#ff5500`).

### Test 3 — Protocol Auto-Detection Verification
Run:
```bash
./ashi_view -i samples/star_transparent.png
```
**Expected Observation:** The `Protocol:` line in the info output accurately identifies your active terminal emulator's preferred backend.

### Test 4 — Full Animation Playback
Run:
```bash
./ashi_view -w 35 -s 2.0 -n 2 samples/spinner.gif
```
**Expected Observation:** Plays the spinner at 2× speed for exactly two cycles and exits cleanly.

---


congratulations! you have rebuilt **ashi_view** from the ground up and mastered terminal escape protocols, raw image decompression, box resampling, double-pixel rasterization, and animated frame pacing in C.
its been a long journey ... share it with anyone who wants to learn.. 

HOPE YOU LEARNED something from it. i will going to be making more of these guides stay tuned....
