# Phase 4 — The Unicode Half-Block Rasterizer & ANSI State Machine

## 1. Goal
NOTE: 
this is gonna be a very IMPORTANT, long phase and guide, you will have a lot of questions and gaps , although i tried to cover everything extensively here and also added bunch of reff links to read and deep dive from , but you may have some other queries : if thats the case then i encourage you to use ai to your advantage send this guide + the reff code as a prompt and ask whatever uh want ..
this is a great way to learn and move on...

In this phase, you will build the primary universal rendering engine of `ashi_view`: the **Unicode Half-Block TrueColor Rasterizer** (`render_halfblock.h` and `render_halfblock.c`).

these names will make sense as you read more, but to put simply : to create a rectangular grid of unicode half blocks [ like `▀`] which are colored somehow.

By pairing the Unicode upper half-block character (`▀`) with 24-bit ANSI foreground and background colors, you will achieve 2× vertical subpixel density, turning any standard terminal into a high-fidelity image display. You will also implement an **ANSI escape deduplication state machine** and a 64KB I/O buffer to deliver blazing-fast rendering speeds without terminal emulator lag.


---

## 2. What You Should Understand Before Coding


### what is unicode and why [you can skip some theory but its always good to atleast have an idea about this]

unicode is a universal coding standard designed to give every single character, letter, symbol, and emoji across the world a unique number, no matter the computer platform, program, or language.
in the early days of computing, computers used a system called ascii, which could only store 128 characters , basically just standard english letters, numbers, and basic punctuation. if you wanted to display special symbols, math operators, or characters from other languages like japanese, arabic, or hindi, old computers would break and display garbled text because they lacked a shared system.
unicode fixed this by creating a massive, single directory that currently maps well over 100,000 unique characters. it allows computers everywhere to agree that a specific number always represents the same exact thing. in our tool, we use unicode because standard ascii doesn't have the special half-block shapes (▀ and ▄) we need to trick the terminal into drawing pixels.[more on that next]

### why unicode here?

unicode is used in this ashi_view , 
 when a user is running a standard terminal environment that doesn't support direct graphic rendering protocols like kitty or iterm2. when someone uses a regular console, tmux, or connects over ssh, the tool cannot send raw image data directly to the screen. instead, it falls back to a clever trick that converts the image into text blocks, and this is exactly where unicode comes into play.


we use specific unicode characters specifically the upper half-block ▀ and the bottom half-block ▄ to solve a physical limitation of terminal text fonts. normally, terminal characters are twice as tall as they are wide. if you tried to display a square pixel using a full solid block character, the image would look squashed and heavily distorted.

by printing the unicode upper half-block ▀, we split a single tall character cell into two perfect, square subpixels. we can then use standard terminal text commands to set the foreground color for the top half and the background color for the bottom half. unicode is what gives us the building blocks to double our vertical resolution and display sharp, proportional images out of pure text.


### what is raster and why its needed here:

a raster in comp sci and graphics is a rectangular grid of discrete picture elements called pixel where each individual cell stores specific color, intensity, or data values.

things to know ;)

• Pixels: the smallest addressable units of a raster grid, holding individual color components (such as Red, Green, and Blue --> RGB)

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

in function render_frame_halfblock that we are gonna write, we run a nested loop that moves down the rows and across the columns of the image. inside this loop, we calculate the memory offset using the formula `(y * cols + c) * 4`. this points us directly to the red, green, blue, and alpha values for both a top pixel and a bottom pixel.

in function blend_pixel that we are gonna write, we check if those pixels have transparency. if they do, we use math to blend the pixel colors with a solid or checkerboard background before drawing. once we have the clean colors, we use standard ansi text codes to paint the top color as the foreground and the bottom color as the background, then.. we print a half-block character.

we needed this raster approach in the first place because an image file is made of thousands of tiny squares, but a terminal only understands rows of letters. by resizing the image into a specific raster grid size that matches the terminal window,

we can convert those raw color bytes into colored text blocks that trick your eyes into seeing a real photo...[cool ...very cool indeed]

thats pretty much the gist of it , lets talk more about concepts and techniques invoved

### about use of hex:

when you see `\x` inside the code, it stands for hexadecimal (hex for short). it is a direct way for a programme to tell the computer exactly what raw bytes of data to send down the pipeline, rather than typing out human text.  
in c, if you write a normal letter like "a", the computer understands it perfectly. but if you want to write a special symbol from the Unicode Standard like our half-blocks—you cannot easily type that with a standard keyboard. instead, we use \x followed by two numbers or letters to represent the exact raw byte value.  

look in code:

• `\xe2\x96\x80` is the exact byte sequence for the upper half-block ▀ character.  
• `\xe2\x96\x84` is the exact byte sequence for the lower half-block ▄ character  
• `\x1b` is the raw hex value for the escape code, which tells the terminal window that a color control command is coming instead of normal printable text.  

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


<img width="2603" height="816" alt="pixel divs" src="https://github.com/user-attachments/assets/907505fe-7a25-41df-aa5b-393661a1bd09" />

<img width="2615" height="1195" alt="img" src="https://github.com/user-attachments/assets/a06003f3-74c0-4198-b7db-4c9163fd915a" />

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

### the escape bandwidth problem & differential state machine

A naive renderer emits both foreground and background escape sequences for every cell:
$$\sim 40 \text{ bytes per cell} \times 12,800 \text{ cells} = 512 \text{ KB per frame!}$$
terminals spend significant CPU time parsing escape strings. When thousands of identical escapes are sent, the terminal lags.

#### solution: a state machine
maintain the currently active terminal colors in state variables:
```c
int cur_fg_r = -1, cur_fg_g = -1, cur_fg_b = -1;
int cur_bg_r = -1, cur_bg_g = -1, cur_bg_b = -1;
```
* if the top pixel has the same color as `cur_fg`, **do not emit a foreground sequence**.
* if the bottom pixel has the same color as `cur_bg`, **do not emit a background sequence**.
* on photographic images, skies, gradients, and solid areas, this reduces the output payload by **60% to 85%**.

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
$$\text{checker} = \left(\left\lfloor \frac{x}{2} \right\rfloor \oplus \left\lfloor \frac{y}{2} \right\rfloor\right) \text{ AND } 1$$


  Alpha blending formula:
  $$C_{\text{out}} = \frac{C_{\text{src}} \times A + C_{\text{bg}} \times (255 - A) + 127}{255}$$
* **Native Terminal Pass-Through (`--bg none`):**
  If a pixel is fully transparent ($A < 32$), we do not blend it. Instead:
  * If both subpixels are transparent: output space `' '` with reset color.
  * If top is opaque and bottom is transparent: output `▀` with `\x1b[49m` (default BG).
  * If top is transparent and bottom is opaque: output `▄` (`\xe2\x96\x84`, Lower Half Block) with `\x1b[49m`.
  The user's actual terminal background color shines through.

### if you had a panic attack reading about alpha-blending then read this ;)


#### alpha blending: basics + the big idea

imagine you have a sheet of clear glass. if you paint a solid, thick layer of red paint on it, you cannot see anything behind it. but if you take that red paint, mix it with a lot of water, and brush it on, the glass becomes a translucent red tint. you can still see the objects behind the glass, but their colors are now mixed with the red paint.
in digital graphics, alpha blending is the exact mathematical process of mixing a foreground color (the transparent paint) with a background color (whatever is behind the glass) based on an alpha value
The alpha value represents the level of opacity:

* $\alpha = 255$ (or $1.0$): Completely solid/opaque. You only see the foreground color.
* $\alpha = 0$: Completely see-through/transparent. The foreground color is invisible.
* $\alpha = \text{anything in between}$: Semi-transparent. The final color is a mixed combination of both.

#### lets derive the blending code

if you are writing a graphics engine from scratch, how do you derive the math? let's think it through those steps

thought 1: physical concept

if an alpha value represents a percentage of opacity, then the final color should be:
$\text{final color} = (\text{foreground color} \times \text{percentage of opacity}) + (\text{background color} \times \text{percentage of visibility left})$

if opacity is 70%, then the background should show through by 30%. that means: [as the total is 100%]
$\text{visibility left} = 100\% - 70\% = 30\%$


thought 2: translating percentages to computer bytes

computers don't naturally calculate percentages like \(70\%\); they store color channels (red, green, blue, alpha) inside 8-bit unsigned integers (uint8_t). an 8-bit byte ranges from 0 to 255.[as we already know]  
so, instead of \(100\%\), our maximum capacity is 255.  
• if foreground alpha is \(a\), then the background visibility multiplier is exactly 255 - a.  
let's write out that formula using our integer scale:  
$$\text{final} = \frac{(\text{foreground} \times a) + (\text{background} \times (255 - a))}{255}$$


thought 3: fixing the integer division flaw

in computer programming, integer division always cuts off the decimal remainder (it rounds down). if our division calculates 10.99, the computer drops the decimals and treats it as 10.   over an entire image, this constant rounding down makes colors look slightly muddy and darker than they should be.  
to fix this, we need to round to the nearest whole number. the trick in computer science is to add half of the divisor to our total before dividing. since we are dividing by 255, half of that is roughly 127.  
adding 127 balances out the truncation flaw perfectly:  
$$\text{final} = \frac{(\text{foreground} \times a) + (\text{background} \times (255 - a)) + 127}{255}$$

and ..... just like that we have derived our blending engine  

#### how alpha blending happens in our code

let's look at how this logic is structured inside blend_pixel.

the datatypes i used

• const uint8_t *p: a pointer to the raw array holding our source pixel colors. p[0] is red, p[1] is green, p[2] is blue, and p[3] is alpha.  
• uint8_t a: stores the single byte alpha value of our foreground pixel.  
• uint8_t bg_r, bg_g, bg_b: separate bytes holding the red, green, and blue values of our selected background layer.  
• uint32_t: we temporarily cast our bytes into 32-bit unsigned integers during multiplication. if we multiply two 8-bit bytes together $255 \times 255 = 65,025$
, the result is too large to fit back into an 8-bit byte (which caps at 255). upscaling to uint32_t prevents data overflow.  

#### the flow of the code

inside blend_pixel, we extract the alpha value
``` uint8_t a = p[3];```
1. first, we check for extreme shortcuts to save computing

power. if the alpha is essentially zero `(a < 32)`, we flag the pixel as transparent `*out_is_trans = true;` and exit.   if the alpha is completely solid `(a == 255)`, we just copy the raw pixel colors directly to the output variables and skip the blending part entirely.  
2. if the pixel is semi-transparent, we find out what background color is sitting beneath it. if the tool configuration says `BG_CHECKER`, we use bitwise math ```((x >> 1) ^ (y >> 1)) & 1 ``` to generate an alternating grey checkerboard pattern.  
[i will explain this later , if its is troubling you then scroll down and read the xor + checkerboard section after reading this]  


3. finally, we run our rounded integer division formula to blend the red, green, and blue values:
```c
uint32_t inv_a = 255 - a;
*out_r = (uint8_t)(((uint32_t)p[0] * a + (uint32_t)bg_r * inv_a + 127) / 255);
```



#### why do we need this technique here?

images like png files frequently use transparent edges, dropped shadows, or semi-opaque backgrounds. terminal screens do not naturally support transparent layers for characters. if we didn't perform this mathematical blending manually inside our code, transparent sections would render as solid, ugly blocks of black or white pixels

#### shockinglly its a standard industry application

this is a foundational technique known as linear interpolation blending. it is used universally across game development frameworks (like Unreal Engine), web browsers drawing transparent elements on a screen, graphic design software like adobe photoshop, and modern ui engines like flutter or apple's swiftui.
[more info](https://courses.grainger.illinois.edu/cs418/fa2024/text/lerp.html)

#### main project functions
if you are writing code and uh find yourself confused about the flow and big-picture then read this once

 helper fxn
```
src rgb[bg+fg]    -->   string of hex rgb[bg+fg]  
  
```
render_frame_halfblock
```
src pixel array ---helper fxns---> final printable strings
                                        [in 64kb buffer]
```

##### 1. color injectors: outbuf_fg_rgb and outbuf_bg_rgb  

these two small helper functions act as our color pens. their single job is to construct the text-based ansi escape sequences that tell the terminal window to switch its current text colors.    
• outbuf_fg_rgb: takes a reference to our 64kb buffer along with three color numbers `(r, g, b)` and builds a string matching `\x1b[38;2;R;G;Bm`. this modifies the color of the foreground text character itself.  
• outbuf_bg_rgb: does the exact same thing but constructs the sequence `\x1b[48;2;R;G;Bm`. this modifies the color of the solid rectangular text block background.  
both functions use snprintf to print the formatted text sequence directly into a small temporary character string, then append that string straight onto our 64kb output storage layout.  

##### 2. the internal row loop: render_frame_halfblock  

this function takes a flat, single-frame array of scaled pixels and transforms it into the final printable text strings.

this function is important , thats why i'll be a littile thorough with it...

i. it runs a outer loop moving downward through the rows. because one character cell holds two vertical subpixels, it maps a single loop pass index r to a y_top image row and a y_bot image row.  
    i want you to know that this r or loop tracks the standard character rows of our terminal.[row0 , row1 , row2 .... so on]  
    but , our img raster grid has twice as many pixel rows as the terminal has text char rows!  
  - to map those image pixels down into our text characters without crushing our picture, every single time the loop processes one text row r, it must grab two rows of pixels from our image data.  

so in working:
  when the loop is on text row `r = 0` (the very top line of text):
  • `y_top = 0 * 2 = 0` (grabs row 0 of the image pixels)
  • `y_bot = 0 * 2 + 1 = 1` (grabs row 1 of the image pixels)
  • result: terminal row 0 prints image pixel rows 0 and 1 together inside its half-blocks.

  and so on...

ALSO INITIALIZE STATE VARIABLES HERE.[we've talked about them above]  

one question you can haev is why did i used -1 here as the value of state var[rgb] for both bg & fg?

to explain this  
in our terminal rendering engine, color values for red, green, and blue can only be numbers from 0 to 255. because a color component can never physically be a negative number, setting them to -1 guarantees that whatever real pixel color comes in first is guaranteed to be completely different from our state variables.
flow: 
1. fresh line: at the start of every single row loop, the terminal cursor has just jumped down to a brand-new line. the terminal window doesn't know what colors we were using on the previous line; it resets its drawing pen.
2. triggering the first write: when our inner loop looks at the very first pixel of that row, it compares the pixel's true red color (let's say it is black, which is 0) against our state tracking variable cur_fg_r (which is currently -1).
3. the conditional check: inside our loop code, we run this check; ```if (cur_fg_r != tr || ... )```  
since 0 does not equal -1, this check evaluates to true. the engine immediately jumps inside the block, calls outbuf_fg_rgb to write the color commands to our 64kb buffer, and then updates our state variable:  
```cur_fg_r = tr; // cur_fg_r changes from -1 to 0```

Ohhh and why it wasnt 0?
bcuz..  if i had initialzed these state variables to 0 instead of -1, and if the first pixel of our image happened to be pure black (0, 0, 0),   
our system would check `cur_fg_r != tr (0 != 0)`, evaluate it as `false`, and falsely skip printing the color command. your terminal would end up rendering that first pixel using whatever random text color it already had left over in its active memory buffer, corrupting the image.




ii. an inner loop moves left-to-right across the horizontal columns (c).

iii. for every step, it uses the flat array math `(y * cols + c) * 4 ` to point to the exact memory locations of both the top and bottom subpixels.  
   after that , 
   to extract the raw color data from memory for the exact two pixels we want to draw, and then create placeholders to store their processed colors.we do this
   
what are `tr, tg, tb, br, bg_col, and bb`?

these are variables that hold the individual red, green, and blue (rgb) color channels for our two stacked subpixels after they have been processed for transparency.
• the t prefix stands for top pixel:
```
  • tr = top pixel red
  • tg = top pixel green
  • tb = top pixel blue
  ```
• the b prefix stands for bottom pixel:
```
  • br = bottom pixel red
  • bg_col = bottom pixel green (note: i named it bg_col instead of bg because bg was already being used for our background configuration structure!)
  • bb = bottom pixel blue
```
why do we need these variables right here?

when we fetch `p_top` and `p_bot` using our pointer math, we are pointing to the raw, unprocessed pixel data straight out of the file layout. these pixels might contain transparency data (alpha channels).
we cannot send raw transparent pixel bytes directly to the terminal screen because the terminal doesn't know how to blend them. we have to process them first.
so, we create these clean temporary variable slots (tr, tg, tb, etc.) and pass their memory addresses into our blending engine right below that loop step:
```c
blend_pixel(p_top, c, y_top, bg, &tr, &tg, &tb, &top_trans);
blend_pixel(p_bot, c, y_bot, bg, &br, &bg_col, &bb, &bot_trans);
```

the `&` symbol means we are handing the keys (pointers) of these variables to `blend_pixel`. that function does the heavy linear interpolation math we talked about earlier, strips away the transparency, and writes the final, solid rgb values directly into those variables.
once `blend_pixel` finishes its work, our column loop reads those exact variables (tr, tg, tb, etc.) to know exactly what text color sequences to write into our 64kb buffer.





iv. it calls blend_pixel for both components to blend transparency.

v.  it compares the results against our state tracking variables (cur_fg_r, cur_bg_r, etc.). if the color values match what we already printed before, it skips writing the ansi sequences to save bandwidth.

vi depending on whether the pixels are solid or transparent, it writes either a space (' '), a top block (▀), or a bottom block (▄), appending them to our 64kb array.

vii. when an entire horizontal row is finished, it inserts a newline character (\n) and repeats until the frame is fully drawn.

##### 3. the control driver: ashi_render_halfblock

this is the high-level manager function that dictates the lifetime of our entire rendering process.  
• it queries the terminal's system dimensions using terminal_get_size to ensure our graphic fits inside the active command-line window boundaries.  
• it runs structural scaling calculations using ashi_calc_fit_dimensions. it passes a hardcoded cell aspect ratio parameter of 2.0 because our custom half-block drawing technique doubles our vertical canvas resolution.  
• it provisions an optimization loop that resamples and prepares every single frame of an image or animated gif into memory before playing it. this guarantees a smooth playback speed with minimal cpu frame pacing hiccups.  
• if dealing with a static photo, it invokes render_frame_halfblock a single time and cleanly exits.  
• if managing an animation loop, it initializes a timing state machine. it cycles through the pre-allocated frames one after another, draws them, calculates the exact execution delay time via terminal_time_ms, sleeps the execution thread to pacing requirements, and resets the terminal cursor position upward to overwrite the old frame cleanly without causing display flickers.  

#### if uh want to deep-dive :

and expand your understanding of color engines and digital layout parsing 
[read alpha compositing mechanics Wiki](https://en.wikipedia.org/wiki/Alpha_compositing)

[read color blending mechanics through W3C](https://www.w3.org/TR/compositing-1/)

[read how terminal process color controls on the ANSI Escape Code Standard Guide](https://notes.burke.libbey.me/ansi-escape-codes/) 
[these guys are awsssome]

### xor logic + checkerboard
if uh had problems understanding the checkerboard logic or xor statement we did above then please read this..

now listen if you look at `include/ashi.h` there's the data layout[struct] that stores the user's choice for what should appear behind a transparent img
`ashi_bg_color_t` holds the config.

1.  `BG_NONE`: do not draw any background at all (leave the terminal window transparent).
2. `BG_SOLID`: draw a solid background color using the specific r, g, and b values.
3. `BG_CHECKER`: draw an alternating checkerboard pattern of dark grey and light grey squares.[you see that in many standard image viewers]

### lets talk more about it
#### checkerboard -
if you've ever used an image editing app like adobe photoshop or gimp, you have seen the checkerboard pattern before. it is a grid of alternating light grey and dark grey squares.

in computer graphics, we use a checkerboard pattern as a visual indicator for transparency. because transparency is invisible, you cannot see it directly. if we just left transparent pixels blank, they would show the black color of your terminal screen. if your image also has black pixels in it, you wouldn't be able to tell where the image ends and where the empty background begins!

the checkerboard grid gives your eyes a clear pattern so you can immediately see which parts of a png image are solid and which parts are completely see-through.


#### how do we calc a grid without a map?

if you want to draw a checkerboard grid, how do you know whether a specific pixel coordinate (x, y) should be painted light grey or dark grey? let's do my fav thing - derive the logic from absolute scratch.

thought 1: the alternating row and column rule

on a standard chess or checkerboard, the colors alternate every time you move one step to the right, or one step down.
1. if you are on column x, whether it is odd or even tells you its stepping pattern. x % 2 will flip between 0 and 1.
2. if you do the same for row y, y % 2 flips between 0 and 1.
if you combine these two states using an exclusive or (xor) operation, written as the `^`symbol in c .. you get a perfect checkerboard pattern that switches every single pixel. an xor statement outputs a true value (1) only when the two inputs are different from each other.

```c
int checker = (x % 2) ^ (y % 2);
```
it creates an alternating checker pattern of single pixels


thought 2: making the squares bigger

a single-pixel pattern is incredibly tiny and looks like grainy television static on a screen. we want our checkerboard squares to be larger and easier to see. let's make each square 2 pixels wide and 2 pixels tall.

to do this, we need to slow down how fast our numbers change. instead of checking if x is odd or even, we want `x = 0` and `x = 1` to behave exactly the same way. then we want `x = 2` and `x = 3 `to behave the same way.
in computer science, dividing an integer by 2 drops the decimal remainder. look at what happens:
```
• 0 / 2 = 0, 1 / 2 = 0 (both give 0)
• 2 / 2 = 1, 3 / 2 = 1 (both give 1)
• 4 / 2 = 2, 5 / 2 = 2 (both give 2)
```
by dividing x and y by 2, we group our pixels into blocks that are 2 units wide and 2 units tall!


thought 3: optimizing for high performance with bitwise shifts

dividing numbers inside an inner loop that runs millions of times for an animation is slow for a CPU. inside computer processors, data is stored in binary bits. shifting all the bits of a binary number one slot to the right is mathematically identical to dividing that number by 2, but it executes almost instantaneously.
the symbol for shifting bits to the right in the c language is `>>`
so we are doing bit manipulations.
```
• x >> 1 means "divide x by 2".
• y >> 1 means "divide y by 2".
```
and hence the code we wrote
```c
int checker = ((x >> 1) ^ (y >> 1)) & 1;

```
##### so in short
note: this x and y  values are exact horizontal and vertical coordinates of the specific subpixel we are currently rendering.

x = column position or (how far right the pixel is from the left edge of the image).

y = row position or • (how far down the pixel is from the top edge of the image).

SO X AND Y are given to blend pixel function from the rendering function[render_frame_halfblock]
as uh may have read above.


1. x >> 1 and y >> 1:  shrinked our coordinates down by half, forcing pixels to stick together in groups of two so our checkerboard blocks are larger.
2. The XOR Operator (^): compared the two grouped coordinates. if one coordinate group is even and the other is odd, it returns a number ending in binary 1. if they are both even or both odd, it returns a number ending in binary 0. This creates the diagonal alternating pattern.
3. The Bitwise AND Mask (& 1): isolated just the very last binary bit of our calculation. it guarantees that our final checker variable is strictly clamped to a clean, simple value: either 0 or 1.

AND once that line runs, we use a quick conditional check to assign the actual colors:

```c
bg_r = bg_g = bg_b = checker ? 0x38 : 0x24;

```
if checker is 1, we use the hex color value 0x38 (a lighter slate grey).
if checker is 0, we use the hex color value 0x24 (a darker charcoal grey).

becuz this line executes entirely using raw binary bit manipulation, it calculates the background color grid instantly without requiring any memory storage or image files, keeping the rendering light and fast

i hope you understood that , if not i will reccomend and push you to go and google stuff out , figure things on your own , whatever i wrote here are mostly fillings for my own gaps of understanding .i or anybody can only do so much to help you , you have to fill your unique set of gaps on your own .. ;) and thats learning isn't it...

### about 64kb buffer
when i designed this engine, one of the biggest performance bottlenecks i had to solve was terminal rendering lag.
to fix this, i built a custom stack-allocated caching buffer called OutBuf. here is the technical breakdown of what it is, 
why we absolutely need it, and what it holds under the hood.

1. what is its datatype?

the buffer is defined using a simple custom structure (struct) that pairs a raw memory array with a size tracker:
```c
typedef struct {
        char data[65536];
            size_t len;
} OutBuf;
```

• data[65536]: a flat array of standard 1-byte characters (char). it is exactly 64 kilobytes in size ($64 \times 1024 = 65,536$ bytes)  

• len: a size counter (size_t) that tracks exactly how many bytes of text are currently sitting in the array at any given second.

2. why do we need it here?
as explained above
in basic programming, you might use printf() or putchar() every time you want to output a character or a color change. in a high-performance graphics viewer, doing this is fatal.  
every time your program communicates with the terminal window, the operating system has to perform a system call (syscall).   syscalls are incredibly expensive operations for the cpu.   
if our terminal grid is $160 \times 80$  
 that is 12,800 cells. if we perform multiple print commands per cell, 60 times a second for an animation, we throw thousands of tiny syscalls at the system. this results in severe screen tearing, dropping frames, and massive cpu spikes.  
the 64kb buffer acts like a staging warehouse. instead of sending out thousands of tiny data packets, we bundle everything up into one massive shipping container in memory. only when that container is full (or the entire frame is done rendering) do we make one single syscall using fwrite(). this bypasses the standard c formatting overhead and flushes everything directly to the terminal parser at maximum speed.

3. what exactly does it hold?

it is crucial to understand that this buffer does not store raw pixel data (rgba values). by the time data enters OutBuf, all the mathematical pixel calculations, alpha blending, and color state tracking are already finished.  
this buffer holds raw, text-based terminal commands and symbols encoded as bytes. specifically:  
• ansi truecolor strings: raw ascii text sequences like
```
 \x1b[38;2;255;120;0m (foreground color) and \x1b[48;2;32;32;32m (background color).  
```  
• unicode half-block graphics: the raw three-byte strings `\xe2\x96\x80 (▀)` and `\xe2\x96\x84 (▄)` which represent the physical subpixels.  
• structural console characters: plain text spaces (' ') for completely transparent segments, reset codes `(\x1b[0m)`, and newlines `(\n)` at the end of each graphic row.

4. how it works in the rendering loop  

in the execution flow, three inline helper functions manage this buffer:  
1. outbuf_append: drops text strings into data at the current len index. before copying, it checks if adding the new text will overflow the 65,536-byte limit.  
2. outbuf_flush: if the buffer is about to overflow (or the frame is finished), this function triggers fwrite(b->data, 1, b->len, stdout). this dumps the entire memory segment to the console and resets len back to 0.  

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

the main purpose of this function in ashi_view is speed. in standard programming, you usually use a function like printf to show text on the screen and printf is slow because it has to look through your text, format variables, and send data to the terminal one tiny piece at a time.

if you do that for thousands of colored pixels on a screen 60 times a second, your program will freeze, lag, or tear.

instead, we created our custom OutBuf system. in function outbuf_append that we are gonna write, we pack all our raw text, unicode shapes, and \x color codes into a massive 65,536-byte tray in memory. then, in function outbuf_flush that we are gonna write, we use fwrite to dump that entire tray to the screen in one single, lightning-fast action. it bypasses all formatting overhead and draws the whole frame instantly.

it can be a lot confusing , but let it sink in &  you will get it eventually


### External APIs / services
* *None.*

---

## 4. Reference Project Map

Inspect these files in the reference project:

* `include/render_halfblock.h`: declares `ashi_render_halfblock()`.
* `src/render_halfblock.c`:
  * `OutBuf`: stack-allocated 64KB buffer with `outbuf_flush` and `outbuf_append`.
  * `blend_pixel`: Alpha blending for checkerboard, solid, and transparent pass-through.
  * `render_frame_halfblock`: the dual-pixel scanning loop with state deduplication.
  * `ashi_render_halfblock`: main entry point computing fit dimensions, allocating resampled buffers, and invoking the renderer.

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
