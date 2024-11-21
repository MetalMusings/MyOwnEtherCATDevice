# MetalMusings EtherCAT device

This repository contains the EtherCAT devices I have designed, built and put in use in linuxcnc.

Everything should be here - Schematics, PCB layouts, and what is needed to build the boards.
Software tools to create an ESI-file and program the ESC eeprom. 
Software tools to create the application running on a STM32F407VG.

I have (so far) used two different ESC ICs: LAN9252 and Ax58100. 
Both work, I can't really see a difference. 
The Ax58100 has a leg spacing of 0.4 mm, 0.5 mm for the LAN9252.
On paper the Ax58100 has more builtin functionality like stepper motor drivers,
but the documentation is lacking. Or my understanding of the documentation is lacking.
The LAN9252 is the true and tested IC of the two. If you don't know, go for the LAN9252.

I learned most of what I know about EtherCAT through a number of Youtube Videos I made.
There are accompanying git branches. For example, for video 8 you can check out the
Video8 branch to get the software tree in the state at the video.
More info on the videos, which now more are of historical intereste, but also a bit of learning
by following [this link](Videos.md)


## EaserCAT 2000 - a testbench for ESC+MCU, SOES, stepper generator, linuxcnc

This was the first card that got used in Linuxcnc. Follow [link here](Cards/EaserCAT-2000/)
I think I got it working pretty well in the end. There was, and still is, an issue with
synchronization between the linuxcnc servo-loop and the ESC DC loop.
I don't remember the status of it anymore, it was some time ago I worked with it.
The main thing was that it lacked optocouplers and isolation for the external signals.


## EaserCAT 3000 - All in one board with Ax58100

This card is an evolution of the EaserCAT 2000 and was intended to be used in my plasma cutter.
It features four stepper driver outputs, input for a THCAD arc voltage card, an encoder, 
an analog output (for spindle +- 10V), eigth digital inputs, four digital outputs, 
plus some 12 I/O for any possible extension. The IOs are isolated from the MCU.

However, I couldn't get this card to work, almost at all. The Ax58100 didn't even load the eeprom.
Struggling with the documentation for the Ax58100, which formed my negative opinion about that,
I gave up and let it sit for over half a year. It was first when a user came and asked questions
around it had another look. I built a new board, and it all started to work.
At time of writing it all seems to work.
[Link here](Cards/EaserCAT-3000-Digital-Stepper-Analog-Encoder-Frequency/)

## EaserCAT 4000 - THCAD reader

Faced with the problems with the EaserCAT 3000 board, and still wanting to use EtherCAT in
my upcoming plasma cutter, I made this board whos single purpose is to read the PWM frequency
from Mesa's THCAD board. The THCAD board reads the voltage between the nozzle and the workpiece
which is 100V something. Highly isolated, and with a voltage to frequency converter it
delivers a differential pwm signal. The frequency of this signal can be related to the voltage.
That's what is needed for linuxcnc torch height control.

In the tests on the bench this can read up to 200 kHz, but its's better to be below 100 kHz.
It seems to work fine. Implementation is with interrupts. The best option would be to use
TIM2's PWM_INPUT function. I tried a lot but couldn't get it to work. It still works for
its intended use. If time and interests are in phase with the moon, I might look at that again.
[Link here](Cards/EaserCAT-4000-THCAD-Reader/)

## EaserCAT 5000 - Digital IO without MCU

A card, just doing Digital Input and Digital Output. The intentions is to read limit switches
and some other switches around the plasma torch. And to switch on the plasma torch.

It's a single LAN9252, configured for local IO. To IO pins are optocouplers attached for isolation.
At point of writing it has not been run yet. Still waiting for parts.
[Link here](Cards/EaserCAT-5000-Digital-8In-8Out-LAN9252-only/)

###License

Don't violate the original licenses. No warranties. Use it any way you like.
