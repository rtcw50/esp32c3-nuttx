# Watch Cleaner Controller v3

## Introduction

This application is used by my homemade watch cleaner
machine. On power up, the GUI displays on a 320x240
LCD touch screen. 

## Main screen

# Start,Pause Button
Start or Pause the motor. 

# Stop Button
Stop the motor

# Mode Selector

Clean: select the parameters for the clean cycle
Rinse: select the parameters for the rinse cycle
Spin:  select the parameters for the spin cycle

# Preset Button

There are 3 preset buttons. Preset 1 is the default.
Preset 2 is the "light" or quick cycle.
Preset 3 is the "agressive" or long cycle.

The user may define the preset parameters in the settings
screen. See the setting screen description.

# Setting Button (Gear Icon)

Display the settings screen.

## Settings screen

The settings screen contains the controls for the 
clean, rinse and spin parameters.

Clean: set the cleaning time
Rinse: set the rinse time 
Spin: set the spin time
Agitate: set the time interval for the motor to reverse
RPM: set the motor speed 
Spin Up Rate: set the slew of the motor (1 slow to 10 fast)

3 Preset buttons line the settings title bar. 
A short click selects the preset. A long press
sets the button to the current parameter settings.

Done button returns to main screen.

## Calibration

The application saves touch screen calibration parameters
to a local flash file system. If the calibration file
does not exist, the device will enter calibration mode.
The calibration mode will ask a user to touch four points
in sucession. The calibration measures the x,y coordinates
from these four points and creates correction factors so that
x,y coordinates always read between the horizontal (x) 
resolution and the vertical (y) resolution. 

There's an unused GPIO6 pin on the esp32c3-xiao device
from Seeed Studio. When this pin is pulled low with a 
1Kohm resistor, the machine will start up in calibration
mode. You can use this method if you ever screw up the 
calibration routine (like not touching the corners). You'll
know if your calibration is not correct because the screen
won't respond to touches or it won't touch the correct buttons.
