# Phase 1 — Project Foundation & Terminal Control

## 1. Goal

In this phase, you will create the foundation of `ashi_view`: the project structure, the build script (`Makefile`), and the cross-platform terminal control subsystem (`terminal.h` and `terminal.c`).

By the end of this phase, you will have a working binary that queries the terminal window size (columns and rows), toggles terminal cursor visibility, clears the screen, and safely restores normal terminal operation even if the user hits `Ctrl+C`.

---

## 2. What You Should Understand Before Coding

### Monospace Terminal Grid
A terminal emulator displays text on a fixed grid of character cells. Each cell has an address defined by:
* `column` (horizontal position, starting at 1 or 0 depending on the escape sequence)
* `row` (vertical position, starting at top)

Unlike a graphical window measured in pixels, terminal applications must arrange their layout in units of character cells.

### Virtual Terminal (VT) / ANSI Escape Sequences
ESCAPE SEQUENCE — A special sequence of bytes starting with the ASCII Escape character (hex `0x1B`, octal `\033`, or `\e`) that instructs the terminal emulator to perform an action (such as changing text color or moving the cursor) rather than printing literal characters.

Examples:
* `\x1b[2J`: Clear entire screen.
* `\x1b[H`: Move cursor to home position (row 1, column 1).
* `\x1b[?25l`: Hide the text cursor.
* `\x1b[?25h`: Show the text cursor.
* `\x1b[0m`: Reset all text styles and colors to terminal defaults.

### Cross-Platform Console Modes
* On **Linux and macOS (POSIX)**, standard terminals support ANSI escape sequences natively.
* On **Windows 10/11**, the console host (or Windows Terminal) supports ANSI escapes only after your program explicitly enables `ENABLE_VIRTUAL_TERMINAL_PROCESSING` via the Windows Console API (`SetConsoleMode`). Windows also requires setting the console code page to UTF-8 (`SetConsoleOutputCP(CP_UTF8)`) to correctly print Unicode characters.

### Signal Handling
SIGNAL — An asynchronous notification sent by the operating system to a running program when an event occurs (such as the user pressing `Ctrl+C`).

When a user presses `Ctrl+C`, the OS sends `SIGINT` (Signal Interrupt). If your program does not catch this signal, it terminates immediately. If you previously hid the terminal cursor with `\x1b[?25l`, the user is left with a broken, cursorless terminal. A signal handler ensures the cursor is restored before the program exits.

---

## 3. Technologies / Libraries / APIs Used

### We write ourselves
* `include/ashi.h`: Core project constants, enums, and configuration structures.
* `include/terminal.h`: Interface for terminal initialization, dimension queries, cursor controls, timers, and input checks.
* `src/terminal.c`: Platform-specific implementation for Windows Console API and POSIX.
* `src/main.c`: Temporary test runner for Phase 1.
* `Makefile`: Compilation and linking rules.

### Libraries / dependencies
* *None.* (Only standard C library headers: `<stdio.h>`, `<stdlib.h>`, `<stdbool.h>`, `<stdint.h>`, `<signal.h>`).

### Platform / runtime APIs
* **Windows:**
  * `GetStdHandle(STD_OUTPUT_HANDLE)`: Retrieves handle to active console output buffer.
  * `GetConsoleMode` / `SetConsoleMode`: Reads and sets console input/output modes (enabling `ENABLE_VIRTUAL_TERMINAL_PROCESSING`).
  * `SetConsoleOutputCP(CP_UTF8)`: Sets the console output code page to UTF-8.
  * `GetConsoleScreenBufferInfo`: Queries current window width and height in columns/rows.
* **POSIX (Linux / macOS):**
  * `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`: System call to query terminal window dimensions (`ws_col`, `ws_row`).
  * `sigaction`: Registers a clean signal handler for `SIGINT` and `SIGTERM`.

### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project to see how Phase 1 functionality is organized:

* `include/ashi.h`: Defines `ASHI_VIEW_VERSION` and core data types.
* `include/terminal.h`: Declares all terminal manipulation functions and the global interrupt flag `g_terminal_interrupted`.
* `src/terminal.c`: Houses the `#ifdef _WIN32` and `#else` (POSIX) implementations.
* `Makefile`: Configures compilation flags (`-std=c99 -Wall -Wextra -pedantic`) and linking.

---

## 5. Major Tasks

### Task 1 — Create Project Skeleton & Makefile
- 1.1 Create directory tree (`include/`, `src/`).
- 1.2 Write `Makefile` with strict warnings and C99 standard.
- 1.3 Create `include/ashi.h` with basic definitions.

### Task 2 — Design the Terminal Interface
- 2.1 Declare functions for terminal lifecycle (`init`, `restore`).
- 2.2 Declare functions for dimension querying (`get_size`).
- 2.3 Declare functions for cursor control (`hide`, `show`, `home`, `up`, `clear`).
- 2.4 Declare timing and signal mechanisms.

### Task 3 — Implement Platform-Specific Terminal Control
- 3.1 Implement Windows console initialization and dimension retrieval.
- 3.2 Implement POSIX `ioctl` dimension retrieval and signal trap.
- 3.3 Implement cleanup logic to guarantee cursor restoration.

### Task 4 — Write and Run a Phase 1 Test Harness
- 4.1 Write a `main.c` that initializes the terminal, queries dimensions, hides the cursor, waits for user input or timeout, and safely restores the cursor.
- 4.2 Verify signal handling by pressing `Ctrl+C` while the program is running.

---

## 6. Detailed Implementation Tasks

### Task 1 — Create Project Skeleton & Makefile
#### 1.1
Create directories `include` and `src`.
#### 1.2
Create a `Makefile`. Configure compiler variables:
* Use `CC = gcc` (or allow override).
* Add flags: `-std=c99 -O3 -Wall -Wextra -pedantic -Wno-unused-parameter`.
* Set include directory search paths: `-Iinclude`.
* Define output executable name (`ashi_view` or `ashi_view.exe` on Windows).
#### 1.3
Create `include/ashi.h`. Add header guards (`#ifndef ASHI_H`). Define version `ASHI_VIEW_VERSION "1.0.0"`.

### Task 2 — Design the Terminal Interface
#### 2.1
In `include/terminal.h`, declare:
* `int terminal_init(void)`: Sets up terminal mode and signals.
* `void terminal_restore(void)`: Restores terminal attributes and cursor.
#### 2.2
Declare `int terminal_get_size(int *cols, int *rows)`: Writes current terminal columns and rows to the given pointers.
#### 2.3
Declare cursor helper functions:
* `terminal_hide_cursor(void)`
* `terminal_show_cursor(void)`
* `terminal_reset_color(void)`
* `terminal_clear_screen(void)`
* `terminal_cursor_up(int rows)`
#### 2.4
Declare an `extern volatile bool g_terminal_interrupted` flag for signal traps.

### Task 3 — Implement Platform-Specific Terminal Control
#### 3.1
In `src/terminal.c`, use `#ifdef _WIN32` to branch between Windows and POSIX.
#### 3.2
Under `_WIN32`:
* Retrieve standard output handle using `GetStdHandle`.
* Read original console mode using `GetConsoleMode`.
* Enable `ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT`.
* Read and save original code page with `GetConsoleOutputCP()`, then switch to `CP_UTF8`.
* Implement `terminal_get_size` using `GetConsoleScreenBufferInfo`.
* Implement `terminal_restore` to revert original console mode and code page.
#### 3.3
Under `#else` (POSIX):
* Include `<sys/ioctl.h>`, `<unistd.h>`, `<signal.h>`.
* In `terminal_get_size`, call `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`. Extract `ws.ws_col` and `ws.ws_row`.
* Provide fallback to environment variables `COLUMNS` and `LINES` if `ioctl` fails or returns 0.
#### 3.4
Implement signal handler for `SIGINT` and `SIGTERM`. When triggered, set `g_terminal_interrupted = true`, call `terminal_restore()`, and exit with code 130.

### Task 4 — Write and Run a Phase 1 Test Harness
#### 4.1
In `src/main.c`, call `terminal_init()`.
#### 4.2
Call `terminal_get_size(&cols, &rows)` and print:
`Terminal size: <cols> columns x <rows> rows`
#### 4.3
Hide cursor, sleep for 2 seconds (or wait for key), then show cursor and call `terminal_restore()`.

### format should be like around
```
#ifndef _WIN32

//windows api implemented fxns here

#else

//posix api implemented fxns here

```





## 7. Pseudocode

### Terminal Initialization (src/terminal.c)

```
function terminal_init():
    if already_initialized:
        return 0

    on Windows:
        handle = GetStdHandle(STD_OUTPUT_HANDLE)
        GetConsoleMode(handle, &saved_mode)
        new_mode = saved_mode OR ENABLE_VIRTUAL_TERMINAL_PROCESSING OR ENABLE_PROCESSED_OUTPUT
        SetConsoleMode(handle, new_mode)
        saved_cp = GetConsoleOutputCP()
        SetConsoleOutputCP(CP_UTF8)
        register signal handler for SIGINT and SIGTERM

    on POSIX:
        register signal handler for SIGINT and SIGTERM

    mark already_initialized = true
    return 0
```

### Terminal Sizing Query (src/terminal.c)

```
function terminal_get_size(out_cols, out_rows):
    on Windows:
        if GetConsoleScreenBufferInfo(handle, &csbi) succeeds:
            *out_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1
            *out_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1
            if *out_cols > 0 and *out_rows > 0:
                return 0

    on POSIX:
        if ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0:
            if ws.ws_col > 0 and ws.ws_row > 0:
                *out_cols = ws.ws_col
                *out_rows = ws.ws_row
                return 0

    // Universal fallback
    read env variable "COLUMNS" and "LINES"
    if valid integers found:
        *out_cols = env_cols
        *out_rows = env_rows
        return 0

    *out_cols = 80
    *out_rows = 24
    return 0
```

### Signal Handler & Cleanup (src/terminal.c)

```
function signal_handler(signal_number):
    g_terminal_interrupted = true
    terminal_restore()
    exit_process(130)

function terminal_restore():
    if not already_initialized:
        return
    print "\x1b[?25h"  // Show cursor
    print "\x1b[0m"    // Reset color
    flush standard output

    on Windows:
        SetConsoleMode(handle, saved_mode)
        SetConsoleOutputCP(saved_cp)

    mark already_initialized = false
```

---

## 8. Build Order

1. Create `Makefile` and `include/ashi.h`.
2. Create `include/terminal.h` with function prototypes.
3. Create `src/terminal.c` with stub implementations (returning default 80x24).
4. Create minimal `src/main.c` that prints hello world.
5. Compile with `make` to verify compiler setup and flags.
6. Implement `terminal_init` and `terminal_restore` in `src/terminal.c`.
7. Implement `terminal_get_size`.
8. Implement signal handlers and cursor controls.
9. Update `src/main.c` to test the terminal functions.
10. Test compilation and behavior.

---

## 9. Testing & Verification

### Test 1 — Compilation
Run:
```bash
make
```
**Expected Observation:** Compilation finishes with exit code 0 and zero warnings.

### Test 2 — Terminal Dimension Query
Run the compiled binary:
```bash
./ashi_view
```
**Expected Observation:** It prints your actual terminal dimensions (e.g. `120 columns x 35 rows`). Resize your terminal window and run it again; the reported numbers should change accordingly.

### Test 3 — Signal Interruption Safety
Run the binary and immediately press `Ctrl+C`.
**Expected Observation:** The program exits cleanly, and your terminal cursor is visible and normal.

---

## 10. Common Beginner Mistakes

* **Invisible Cursor after Termination:**
  * *Symptom:* After exiting the program, the shell cursor has disappeared.
  * *Why:* `\x1b[?25l` was emitted, but the program terminated or crashed before emitting `\x1b[?25h`.
  * *Fix:* Ensure `terminal_restore()` is called both at regular exit and inside signal handlers.
* **Unbuffered/Delayed Escape Sequences:**
  * *Symptom:* Escape sequences don't take effect immediately.
  * *Why:* `stdout` in C is line-buffered. If an escape sequence does not end in `\n`, it sits in the C runtime buffer.
  * *Fix:* Call `fflush(stdout)` immediately after emitting escape codes.
* **Garbled Characters on Windows:**
  * *Symptom:* Box characters or Unicode print as question marks or strange symbols.
  * *Why:* Windows console code page defaults to OEM 437 instead of UTF-8.
  * *Fix:* Call `SetConsoleOutputCP(CP_UTF8)` in `terminal_init`.

---

## 11. Reference Comparison

Compare your work against the reference implementation:
* Compare your `include/terminal.h` with the reference `include/terminal.h`. Verify that function signatures match.
* Compare your `src/terminal.c` with the reference `src/terminal.c`. Check how the reference handles fallback defaults if window size querying fails.
* Notice that the reference implementation maintains static state flags (`s_initialized`) to prevent double-initialization.

---

## 12. Build-from-Memory Checkpoint

Close your editor and reference files. On a piece of paper or scratch file, answer:
1. What does the escape sequence `\x1b[?25l` do? What is its counterpart to reverse it?
2. Why is `SIGINT` handling necessary in a CLI tool that controls the terminal cursor?
3. On Windows, what function call is required before the console will interpret ANSI escape sequences?

---

## 13. Phase Completion Checklist

- [ ] `include/ashi.h` created with version and basic types.
- [ ] `Makefile` compiles clean C99 code with `-Wall -Wextra -pedantic`.
- [ ] `terminal_init` and `terminal_restore` implemented for your operating system.
- [ ] `terminal_get_size` correctly reports terminal columns and rows.
- [ ] Cursor hide, show, and color reset functions work as expected.
- [ ] Signal handler traps `Ctrl+C` and restores cursor visibility.
- [ ] Build-from-memory checkpoint completed.
- [ ] Git diff reviewed.

---

## 14. Git Commit

### What to commit:
* `Makefile`
* `include/ashi.h`
* `include/terminal.h`
* `src/terminal.c`
* `src/main.c`

### What NOT to commit:
* Binary files (`ashi_view`, `ashi_view.exe`)
* Object files (`*.o`)

### Commit message suggestion:
```
feat: setup project foundation and terminal control subsystem
```

After this commit, proceed to [Phase 2 — Image Decoding & Memory Representation]
