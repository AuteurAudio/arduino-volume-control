# Arduino Based Volume Control System

This is a system for controlling a PGA2311 or similar volume
control with an Arduino.  It is quite featureful, with pushbutton
mute, multiple supported channels (relay switched), compile time
balance, etc.  Will run on most any Arduino with enough I/O for 
the features that desire enabling.  This is a complete large system - 
you will have to pull out what you need for a reduced feature set.
 

## Supported features

 * Rotary encoder volume control with push button mute
 * Control of PGA2311 or similar volume controllers
 * Channel toggle controlling input relay
 * Channel and output dependent EEPROM volume save state
 * Output relay for output headphone isolation / output switching 
 * XOR logic toggle and relay control
 * Volume scaling function for smoother volume transitions
 * Channel balance setting (static or adjustable)
 * Limiting on maximum volume for PGA2311 control registers
 * Digital I/O used: 12 pins (with adjustable balance)
 * Serial debug provided if desired
 * Descriptive commenting
 * Code approximately 10 kB incuding serial debug


## Intended operation

To use this code, you simply need import it to you
Arduino IDE and ensure that the dependent libraries
are available.  You should choose which pins meet your
needs and set them in the #define section.  You can
reverse the operation of some pins, both input switch
pins and output control pins, but setting the Reverse
flag, which is XOR'd with internal logic.

If you are using the PGA2311 functionality, you will
want to pull up the mute pin to logical high with a
suitable resistor so that it is in the mute state on
turn on.  You will also want to make a reasonable RC
filter stage to further debounce your switches and
encoder.  It does not effect performance leaving the
serial debug in the code even when you transition it
away from the USB jumper cable.  Given the use of
Arduino pins 0 and 1 for the serial, you might not 
choose to use any of those for your pins if you are 
using a pre-made Arduino, such as the UNO.

You might find it prudent to trim this code to the
feature set you are looking for rather than use
it with all the conditional compilation.  That would
clean it up quite a bit and make it simpler to debug.
As is clear, this code has a fair bit of flexible
functionality, but it is rather a mess.
 

## IMPORTANT
   
Release of this code in no way implicates that any 
product designed by myself or any company that I am
involved with is constrained in any way to the methods
and / or implications of functionality, expressly or
implicitly, based on the code contained herein.  The code
presented here is intended as an as-is instructional 
collection of potential methods that may or may not be
fit for any application.

