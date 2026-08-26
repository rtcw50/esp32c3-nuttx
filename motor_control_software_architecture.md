# Motor Controller Software Architecture

## Overview

The system is a motor controller board controlling a 12V or 24V DC motor.
The motor is used to drive a parts basket of small watch parts in a cleaning
solution. The system will have a graphical user interface for the "production"
software, but initial development will be done with a text UI. The software 
will use the NUTTX RTOS. There will be at least 2 tasks, one to drive the UI
and the other to control the motor. This system has been created already using
Arduino programming models. One of the goals here is to create an embedded software
system that is more "professional". That is, a software stack that is robust and
explores all the promise and pitfalls of real embedded programming. So it is a 
learning vehicle on how industrial embedded software might be created, rather
than just fun Arduino code. 

## System Requirements

1. System will have a touch screen based user interface and must allow for the following:
    * Set motor speed
    * Set motor run duration
    * Programmable "ramp" time for motor to reach set speed and slow to stopped.
    * Stop motor function (motor ramps down to 0 rpms)
    * Pause function (motor ramps down to 0 rpms temporarily)
    * Resume function (motor ramps up to set speed)
    * "Agitate interval" setting - defines clockwise and counterclockwise spin durations in seconds
    * Spin time setting - motor shall spin in one direction for a set time (for rinse off)
    * Show time remaining in current clean cycle.
2. Motor shall respond in real time at user input.
3. When the start putton is pressed, the clean cycle shall begin, with the motor ramping up to the
   set speed and changing direction at each agitate interval setting. Motor shall ramp up and down 
   according to ramp time settings. The start button icon shall changed from a go arrow to a pause
   symbol while the clean cycle is running. If the button is pressed again, the cycle will pause.
   The symbol icon shall change back to arrow (run). 
4. A separate stop button will stop the cycle and reset the counter to the duration time.
5. UI shall show time remaining while clean cycle is running and update each second.
6. If communication is lost with the UI, the motor shall ramp down to 0 rpms.
7. When the duration of the cleaning cycle is completed, the motor shall ramp to 0 rpms.
8. A radio box widget will determine the mode of the motor (clean or spin).
9. A "Settings" button will change to the UI to the settings page.
10. The settings page will control the motor according to requirement number one.
11. The motor shall react in real time to a change in the settings.
12. The settings shall default to reasonable values.
13. The UI will provide for 3 customizable profiles to be saved persistently across
    power downs of the system. The profiles will be numbered, not named. A save button
    shall save the profile. 
14. There shall be a mechanism to calibrate the touch screen so that a touch corresponds
    to the approximate pixel count in the x and y axis.

(TBD - error codes and test diagnostics to be added)

## System Design

The physical motor control board requires 12 or 24 volts input to drive the motor. The system 
requires 2 GPIO pins to drive a PWM motor control module. The display is a touchscreen color 
display although for the prototyping phase a text UI will be used as a proxy.

The software design consists of 2 tasks under the NUTTX RTOS. Task 1 will handle the UI loop.
Task 2 will handle the motor control state machine. Bidirectional message queues shall be set
up to communicate between the tasks.



