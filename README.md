# Experiments using NuttX RTOS
## Introduction

The Arduino Maker Workshop VScode extension has been great. I successfully developed the "Watch
Cleaner Controller" project, including a GUI using LVGL and am able to drive a motor driver
via PWM controlled pins. The Arduino IDE and Arduino Maker Workshop are essentially the same thing,
except that there's no real debugger on either. The motor controller uses an Espressif esp32c3-xiao
device, which is based on 32-bit RISC-V. 

I wanted to experiment more with programming the esp32c3, specifically understanding and implementing 
tasks via an RTOS. I came across a github repo call cmc-labo/tinyos-rtos, which looked like the 
beginnings of a very simple RTOS that had esp32c3 support. It seems it would be fun to look into
this and try to get it running on my board. In order to help the process along, I found that
the open source RTOS NuttX supported the esp32c3 also. I thought this would be useful in 
understanding how to get an executable built on a different platform from ESP-IDF to load and run
on the device. In fact, NuttX is very helpful in this regard. It uses only the gcc for RISC-V from
the web to build the nuttx executable.   

As it turns out, however, the tinyos-rtos repo is not very complete. 
It does not have the board support needed to create or run esp32c3 executables. Based on comments I 
see in Hackernews, it may be an AI generated project that the author is simply uploading or
is being uploaded by bot agents. I quickly abandoned tinyos-rtos, although I'll monitor the repo
for a while to see if esp32c3 support ever does arrive. 


## Apr 10 2026: Building NUTTX

Building nuttx is quite simple. The OS is very well documented. The documentation for the 
basics of the OS is here: https://nuttx.apache.org/docs/latest/index.html. The esp32c3 specifics are
here: https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32c3/index.html

Get the prerequistes: https://nuttx.apache.org/docs/latest/quickstart/install.html
Install the RISC-V toolchain, instructions are in the esp32c3 link above. 

I used the Seeed Studio XIAO ESP32C3 board.
The esp32c3-xiao board specifics are here: https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32c3/boards/esp32c3-xiao/index.html

To build for the Xiao board:


```
git clone https://github.com/apache/nuttx.git nuttx
git clone https://github.com/apache/nuttx-apps.git apps
cd nuttx
make distclean
./tools/configure.sh esp32c3-xiao:usbnsh
make V=1
```

## Building Custom Applications

I've successfully created a custom app, built it, and executed it on the esp32c3-xiao device.
This video has all the details: https://www.youtube.com/watch?v=O5JmnpkHRlQ
The was a simple copy of hello and put in the apps directory. I created an out of tree
application called "blink" which is detailed below.

Here are some details about a simple "blink" program that is created outside of the 
apps directory source tree. This makes it easier to maintain your nuttx projects outside
of the nuttx and apps directories.

The directory is esp32c3-xiao-apps and is parallel to nuttx and apps. There's a subdirectory
called "blink" in the esp32c3-xiao-apps directory where the custom app lives.
The "blink" program blinks a blue and red LED connected to pins D1 and D2, respectively.
It uses the nuttx way, via ioctl calls to /dev/gpio0 and /dev/gpio1.

There is some setup required for building an app outside of the nuttx apps directory.
The nuttx documentation I followed is here: (item 3) https://nuttx.apache.org/docs/latest/guides/customapps.html
Follow the steps exactly as there are Kconfig, Make.defs, and Makefile in the top level
custom apps directory and the "blink" (or other custom apps) directory. The video above
is helpful, but use this documentation as the source of truth.

The "blink" app uses the gpio configuration to enable CONFIG_DEV_GPIO settings that
allows certains interfaces and macros to be used in the blink_main.c code. So
the configuration must include gpio, e.g. ./tools/configure.sh esp32c3-xiao:gpio.
This will also build the apps/examples/gpio program.

In order to understand what pin /dev/gpio0 and /dev/gpio1 map to, there has to
be modifications to the esp32c3-xiao board files. These are located at 
nuttx/boards/risc-v/esp32c3/esp32c3-xiao. The files modified are include/board.h
and src/esp32c3_gpio.c. Here are the changes: 


```
diff --git a/boards/risc-v/esp32c3/esp32c3-xiao/include/board.h b/boards/risc-v/esp32c3/esp32c3-xiao/include/board.h
index 4ffe981e72..b19d59fe94 100644
--- a/boards/risc-v/esp32c3/esp32c3-xiao/include/board.h
+++ b/boards/risc-v/esp32c3/esp32c3-xiao/include/board.h
@@ -27,10 +27,11 @@
  * Pre-processor Definitions
  ****************************************************************************/
 
+/* Testing */
 /* GPIO pins used by the GPIO Subsystem */
 
-#define BOARD_NGPIOOUT    1 /* Amount of GPIO Output pins */
-#define BOARD_NGPIOINT    1 /* Amount of GPIO Input w/ Interruption pins */
+#define BOARD_NGPIOOUT    10 /* Amount of GPIO Output pins */
+#define BOARD_NGPIOINT    1   /* Amount of GPIO Input w/ Interruption pins */
 
 #endif /* __BOARDS_RISCV_ESP32C3_ESP32C3_XIAO_INCLUDE_BOARD_H */
 
diff --git a/boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_gpio.c b/boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_gpio.c
index 78fe996097..dccd1e7b77 100644
--- a/boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_gpio.c
+++ b/boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_gpio.c
@@ -64,6 +64,30 @@
 
 #define GPIO_OUT1  3
 
+/* GPIO2 is reserved as an IRQ PIN, do not use it for IO */
+#define GPIO3 3
+#define GPIO4 4
+#define GPIO5 5
+#define GPIO6 6
+#define GPIO7 7
+#define GPIO21 21
+#define GPIO20 20
+#define GPIO8 8
+#define GPIO9 9
+#define GPIO10 10
+/* We refer to pins by their Dx names in Arduino, so do it here */
+#define D1 GPIO3
+#define D2 GPIO4
+#define D3 GPIO5
+#define D4 GPIO6
+#define D5 GPIO7
+#define D6 GPIO21
+#define D7 GPIO20
+#define D8 GPIO8
+#define D9 GPIO9
+#define D10 GPIO10
+
+
 #if !defined(CONFIG_ESPRESSIF_GPIO_IRQ) && BOARD_NGPIOINT > 0
 #  error "NGPIOINT is > 0 and GPIO interrupts aren't enabled"
 #endif
@@ -73,6 +97,7 @@
  */
 
 #define GPIO_IRQPIN  2
+#define D0 GPIO_IRQPIN
 
 /****************************************************************************
  * Private Types
@@ -128,7 +153,17 @@ static const struct gpio_operations_s gpout_ops =
 
 static const uint32_t g_gpiooutputs[BOARD_NGPIOOUT] =
 {
-  GPIO_OUT1
+/*  GPIO_OUT1 */
+D1, /* /dev/gpio0 */
+D2,
+D3,
+D4,
+D5,
+D6,
+D7,
+D8,
+D9,
+D10   /* /dev/gpio9 */ 
 };
 
 static struct espgpio_dev_s g_gpout[BOARD_NGPIOOUT];
 ```

 I don't know of a way to pull these changes out of tree. Perhaps
 creating a new custom board and populating it would work. For now,
 accept that you need to customize the files in nuttx work tree.

 What were doing here is adding 10 of the 11 esp32c3 xiao GPIO pins
 to the /dev/gpio tree. D0 is already set to be an interrupt pin, so
 I left that one out as an output. Once the nsh is running, 'ls /dev'
 should show gpio0..9 and gpio10. gpio10 will be the interrupt input
 pin. It would have been nice to have the /dev names correspond to the 
 esp32c3 xiao pinout, and you can do that with an additional mapping
 of pin numbers for xiao in the gpio_pin_register() call. But for now,
 it just uses the ordinal count of the pins to set the /dev/gpio number.

 When you select the "blink" application in make menuconfig, it will build
 along with the other selected apps.  Flashing to the board is done with:
 esptool -c esp32c3 -p /dev/ttyACM0 -b 115200 write-flash -fs 4MB -fm dio -ff 80m 0x0000 nuttx.bin

Run blink in the nsh and the red and blue LEDs with alternate flashing.



## 21 Apr 2026 The ZenC Experiment

I also successfully created a "mypwm" application which registers /dev/pwm0 and /dev/pwm1
devices. Pin configuration is done via the LEDC peripheral configuration. Other
details are located in the mypwm application directory.

Crazy idea: Learn the Zen-C programming language and use it to program the xiao device.
First tests with "Hello World" were successful. 'zc transpile' translates zen-c language
to C11 code. When you use the translated code in a nuttx custom app, it compiles and
works.

There were a couple of hiccups to be aware of. The nuttx build system will emit this 
on the main source compile line: -Dmain=helloz_main. That means, the translated code
from zen-c transpile will have its 'main' function defined as 'helloz_main'. Also
nuttx compile command emits -Werror=return-type. As a result, a simple zenc hello world
code such as:

fn main() {
	println "Hello World"
}

does not return a value and since the 'main' function is renamed to 'helloz_main', the 
nutt risc-v compiler will error out (due to -Werror=return-type), demanding a return value. 
There are a couple of workarounds I see. One, simply name your main function 
'helloz_main' in the zenc file.  The downside of this is it won't link without a 
main function in the host compiler.
Two, just be sure to add a 'return 0' value to the main function so that both the 
host and target compilers are happy. The standard is lenient if the function name
is 'main', so no error is generated. It took a while to figure this out.

So the plan to make the development smooth is to have the makefile in the nuttx custom
apps directory do the 'zc transpile' command to a .c file. Then everything should build
in the nuttx environment. 

It appears the way to do this is to add a new rule to the apps/Application.mk file
to recognize .zc extension, perform a 'zc transpile' and then a normal c compile.
In apps, add the ZENCEXT ?= .zc to Make.defs
In app, Add the ZENC compile and file macro for the ZENC transpiler
In tools, add Zenc.defs file - adds ZENC compiler and ZENCFLAGS flags defs
In tools/Config.mk, add COMPILEZENC macro.
In arch/risc-v/src/common/Toolchain.defs, add include Zenc.defs

Next, how to clean intermediate files.
In tools/Config.mk, add EXTRA += *.zcc to clean zen-c intermediate files.

## 29 May 2026  Abandoning ZenC, Introducing C3

ZenC, while interesting, is a bit immature at this point. Some the ideas
are great, but I found that main issue I have is that there is no 
mechanism to localize symbols in a ZenC module. It needs a flag something
like:
```
zc -private build file.zc
```
which would essentially make the symbols in a module static. An 'export'
keyword would expose the symbol beyond the module. In NUTTX, when you
build the 'nsh' application and want to include multiple ZenC applications,
you'll get multiply defined symbol errors. This was kind of the showstopper
for me and ZenC. 

Because I like coding pain, I guess, I started looking into Zig and another
language call C3. Zig, although not as intimidating as Rust, is still a big
syntax jump from C code. NUTTX has some built-in support for Zig, so there
was that. Because I like pain, I chose to try to port C3 code to NUTTX.
Believe it or not, I finally did it. Here's the story.

https://c3-lang.org says "C3 is an evolution, not a revolution: the C-like
for programmers who like C." The syntax is familiar and comfortable. It does
not take a giant leap to learn the basics. Delving a bit deeper into C3, 
I saw that it compiles down to LLVM IR, those allowing it to target a
variety of CPU/MCU platforms. Since my target is RISCV-32, it could 
certainly support the 32-bit RISCV target. With embedded projects,
code size is always something to consider. My target has a 4M flash
code space, but only 400Kb of SRAM for data, stack, and heap. It was
a real question whether C3 could be used for an embedded application.
Then there was the NUTTX build environment. I had successfully shoe-horned
in ZenC support to NUTTX's makefile system. I did not know if I could
do the same for C3. 

I found that C3 compiles its standard libraries each time it compiles
a user module. Ultimately you end up with object files for all the 
standard library modules you use. There's also option to use external
libc libraries. This was perfect for NUTTX, which provide a platform
specific libc implementation. So far, so good. 

So, I shoe-horned the C3 build into the NUTTX makefile environment.
A set of standard library object files are created by the build along
with the application object file. C3, however, declares many (most) of
its public symbols as weak. As a result, the final link of the 'nsh'
or even the standalone executable for NUTTX will have resolved symbols
that resolve to address zero. This, of course, will result in 
runtime crashes. What I ended up doing is finding all the weak,
global symbols in the object files created by C3 compile and
the forcing those symbols as undefined to the linker, via the -u
flag. This forces the resolution of the symbols to valid linked
addresses. This is especially important for libc symbols extracted
from the NUTTX target platform libraries.

Another critical item is switching to C3 version 8. I used the
'szmageddon' branch and built it locally. This version does a 
couple of key things. One is it corrects a 64-bit 32-bit size
error in LLVM IR GEP for 32-bit targets. Older versions of C3
did not address 32-bit targets properly. Secondly, it now
supports separate sections for functions and data -- important
for dead stripping at link time.

Some additional sections were added to the boards/risc-v/
esp32c3/common/scripts/esp32c3_section.ld file to handle .got,
.got.plt,.tdata,.tbss sections.

With that, and a couple of changes to libc.c3 to ensure stdout()
was properly defined, I was able to execute a native C3 program
on NUTTX on the esp32c3. "Hello World" worked. Now the size is
large compared to a simple C program but we can work out the 
size details later.

There is a nagging problem on the makefile side, however.
NUTTX expects the application object file name to be a fully
qualified path name, e.g., helloc3_main.c3.home.user.Documents...helloc3.o.
As part of the compile, I called a makefile function called RENAME to rename
the object file. There's some type of race condition where the name does
not get changed in time, resulting in a link error. Running make again usually
succeeds. I'll have to look into this again. Supposely putting all the compile
commands on the same line in a recipe forces the entire command to complete
atomically, but this does not seem to be happening.

## Next Steps

The next obvious step is a blink program using C3. The interoperability
with C is key here, since I need to invoke the NUTTX target platform 
libraries to toggle device pins.

## blinkc3 Program

The blinkc3_main program is successfully working. The LED blinks as designed.
The program is located in esp32c3-xiao-apps/blinkc3. Obviously, I needed to 
use the ioctl functions from nuttx target specific libraries. This is easily 
accomplished in c3 by simply declaring the external C functions:
```c
extern fn int ioctl(int fd, int request, int arg);
```
The question of how to handle C defines in a C3 program took a bit more thought.
To import #define definitions into C3, I included a C file into the build and 
create wrapper functions for the defines I need to import.
blink_bridge.c:
```c
#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>

int system_O_RDWR(void) {
  return O_RDWR;
}
```
blinkc3_main.c3:
```c
extern fn int system_O_RDWR(void);
```

This seems to work fine.
Was there any advantage to using C3 for the blink program?
Really not much in this simple program. Using optionals 
to open files was cool and using defer to close files was
helpful. Other than that, it looks like C.

## Back to the Motor Controller

I'm still keen to design my motor controller using proper
RTOS techniques. Ultimately, since I have touchscreen display,
the interface will be developed using LVGL. But I want to 
make progress on motor controller state machine, so I wanted 
to develop a simple text UI to act as the interface in one 
thread and the motor controller to receive commands and report
back status in another thread. 

This led me down another 
rabbit hole in designing a proper text UI. I asked Gemini how
to do this and it reported back to use ncurses. Then I went 
back and told Gemini that we must do this natively using the
termios APIs. Gemini generated a program that worked perfectly on 
a host. The challenge was to port this esp32c3. It pretty 
much ported without issue, but would crash almost immediately.
Recall that in nuttx if you have 'nsh' application, it displays
a prompt from which you can call other modules. A text UI would
compete with nsh for "control" of the picocom terminal. 

But this wasn't the cause of the crasher. I took awhile to 
get the gdb debugger to work, but gdb eventually led me to 
the null pointer access that was causing the problem.
The serial IO for different platforms are abstracted in nuttx.
Each platform must defined the implementations of the interfaces
required to complete serial IO to a terminal or emulated terminal.
In this case, the key file is nuttx/arch/risc-v/src/common/espressif/esp_usbserial.c
The null pointer access was caused by accessing the 'txempty' member of
static struct uart_ops_s g_uart_ops structure. It is set to NULL in esp_usbserial.c.
The function 'esp_txready' provides the exact behavior required (according to Gemini),
so defining 'txempty' to esp_txready was the fix.

So now my text UI works on esp32c3 (mostly). I can boot into a 'nsh>' shell, 
and then execute 'motcontrol'. The termios-based text UI, comes up and responds
correctly. If I exit motcontrol (with the 'exit' command), and then re-run 
'motcontrol', then the text UI does not come up again. I think this will be
solved when I make 'motcontrol' a standalone application. I'll move on to the
state machine development now. 

## Using the Debugger in the Nuttx Environment.

Once I got the debugger working, it worked very well in Nuttx.
Getting there was a bit of a struggle, though. Mainly due to me 
having to go down the learning curve. Here are the steps required.

1. Build Nuttx with debugging enabled
    * CONFIG_DEBUG_SYMBOLS=y
    * CONFIG_DEBUG_SYMBOLS_LEVEL="-g"
2. Start openocd
3. Start riscv-none-elf-gdb
4. Start picocom

### OpenOCD

You must have the espressif openocd installed and in your path. You can do this by
installing the Espressif tools. The command to start is:
```bash
openocd -f board/esp32c3-builtin.cfg -f openocd_fix.cfg 
```
openocd_fix.cfg:
```
# Force OpenOCD to stop presenting the complex memory map to GDB
gdb memory_map disable

# Tell OpenOCD to handle flash programming manually if needed
gdb flash_program enable

# RTOS awareness
set ESP_RTOS nuttx
```

### riscv-none-elf-gdb

```bash
riscv-none-elf-gdb ./nuttx
(gdb) target extended-remote :3333
(gdb) cont
```
To restart the program
```bash
(gdb) monitor reset halt
(gdb) cont
```
**Important** Use 'hbreak' to set breakpoints instead of break.
```bash
(gdb) hbreak motcontrol_main
```
Since we are executing from flash memory, normal breakpoints won't
work since that implies rewriting the instruction at the breakpoint.
You have a limited number of hardware breakpoints, so don't go crazy.

### picocom
The picocom command is:
```bash
picocom -b 115200 /dev/ttyACM0
```
When you 'cont' in gdb, you should get the 'nsh>' prompt in picocom.
You can then Ctrl-C in gdb to set a hardware breakpoint at your module
entry point and then 'cont' again. It should stop at your module.

*Hint* Use ctrl-a, ctrl-x to exit picocom without having to disconnect
any USB cables.

### esptool

As mention earlier, the flash command I use is:

```bash
esptool -c esp32c3 -p /dev/ttyACM0 -b 115200 write-flash -fs 4MB -fm dio -ff 80m 0x0000 nuttx.bin
```
## Back to the Motor Controller - Part 2

July 10, 2026.

I've been struggling with the text UI for some time now, 
but it appears that I have it working as a standalone NUTTX
program. The refresh problems have been solved by separating
the display into 2 panes and refreshing the pane only when 
it becomes "dirty" (changes). This way I can smoothly update
the countdown time display without having to refresh the other
pane. 

So now the main program creates 2 tasks: the motor control
task and the UI task. The tasks communicate via 2 message
queues. The 'run_cleaning_cycle' loop is the heart of the
motor controller. The UI sends a message to "run", and 
the 'run_cleaning_cycle' begins. The loop in the 'run_cleaning_cycle'
sends a message to the UI each seconds with the time remaining so
the UI can update the countdown time display. Within the 'run_cleaning_cycle'
loop, I check for messages from the UI to stop, pause, or abort the cycle.
There is also a check as to whether the agitate_duration time has passed.
This reverses the motor when an agitate_duration time has passed. The
motor always "ramps up" and "ramps down" according to the ramp time
setting. Abort stops the motor immediately.

Still to be done is real time modification of the motor speed.
For this, the 'run_cleaning_cycle' loop will need a message 
from the UI that the motor speed has changed and adjust the target
duty cycle accordingly.

I have seen some crashes. These seems to happen when I input
the commands too fast. Perhaps the message queue gets overloaded.

Next step, get the PWM drivers integrated into the motor control unit.
I believe I'll want a motor_driver.c module that handles the physical
driving of GPIO pins. It will set the motor direction and duty cycle.
I'll use LEDs as the proxy for a motor.

## Back to the Motor Controller - Part 3

I've abstracted the motor driver functions (init, shutdown, start, etc.)
into the motor_driver.c module. I verified that my pipe cleaner program,
mypwm_main.c still works as expected and it does.

On first run of the PWM integration, a crash. Gemini suggests to
set CONFIG_BOARD_LATE_INITIALIZE=y in make menuconfig...

CONFIG_BOARD_LATE_INITIALIZE=y did not work. I ended up doing this:
/* Main App Initialization */
if (board_app_initialize(0) < 0) {
    return -1;
}   
which seemed to work.

I also implemented realtime control of speed, duration, etc. by
adding messages between the TUI and motorcontroller.c:run_cleaning_cycle.

I will abandon the ramp_time parameter and fix the motor ramp up time (
the time it takes for the duty cycle to reach its programmed value or the
time it takes to get to 0) to a permanent value of approximately 1 second.
Each iteration through the run_cleaning_cycle "keep_running" while loop is about
10ms, so I set the MC_RAMP_STEP to 1. This will give enough iteration to ramp 
the duty cycle to its final value in about 1 second. The entire "keep_running"
loop should take at least a second if the timedreceive call were actually blocking
until the next_time time. I don't quite understand how the loop could execute in 10ms.

I increased the stack size of the textui task to 16K. There are large stack
variables in the status and prompt panes that hold the display frame. The
stack was overflowing in some cases, causing a crash. 

Things are now working as expected. The next steps will be to add
profiles (to the a flash memory filesystem), and start implementing 
the LVGL GUI (while removing the text GUI).







