# MetalMusings EtherCAT device

This repository contains the EtherCAT devices I have designed, built and put in use in linuxcnc.

Everything should be here: 
- Schematics, PCB layouts, and what is needed to build the boards.
- Software tools to create an ESI-file and program the ESC eeprom. 
- Software tools to create the application running on a STM32F407VG.
- A big learning curve is hidden in here as well

I have (so far) used two different ESC ICs: LAN9252 and Ax58100. 
Both work, I can't really see a difference. 
The Ax58100 has a leg spacing of 0.4 mm, 0.5 mm for the LAN9252, if that matters.
On paper the Ax58100 has more builtin functionality like stepper motor drivers,
but the documentation is lacking. Or my understanding of the documentation is lacking.
The LAN9252 is the true and tested IC of the two. If you don't know, go for the LAN9252.

The EtherCAT cards, which I call EaserCAT cards, often use an EtherCAT chip and a normal MCU.
I have used the STM32F407VGT6. The EaserCAT chip does the network talk and other 
EtherCAT things. The STM32 does the application things, like reading and setting pins high or low,
reading encoder inputs, generate stepper motor pulses and other interesting applications one can think of.
There is a narrow interface between the two: SPI and a couple of lines for synchronization.
Any decent MCU can do that, it doesn't have to be a STM32F407VGT6. I have tested with a esp32 with success.
Feel free to experiment with other processors.

I learned most of what I know about EtherCAT through a number of Youtube Videos I made.
There are accompanying git branches. For example, for video 8 you can check out the
Video8 branch to get the software tree in the state at the video.
More info on the videos, which are now more of historical interest, but also a bit of learning by following [this link](Videos.md)

## EaserCAT 2000 - a testbench for ESC+MCU, SOES, stepper generator, linuxcnc

![Picture of EaserCAT 2000 board](Utils/Pictures/EaserCAT-2000.png)

This was the first card that got used in Linuxcnc. Follow [link here](Cards/EaserCAT-2000/)
I think I got it working pretty well in the end. There was, and still is, an issue with
synchronization between the linuxcnc servo-loop and the ESC DC loop.
I don't remember the status of it anymore, it was some time ago I worked with it.
The main thing was that it lacked optocouplers and isolation for the external signals.

## EaserCAT 3000 - All in one board with Ax58100

![Picture of EaserCAT 3000 board](Utils/Pictures/EaserCAT-3000.png)

This card is an evolution of the EaserCAT 2000 and was intended to be used in my plasma cutter.
It features four stepper driver outputs, input for a THCAD arc voltage card, an encoder, 
an analog output (for spindle +- 10V), eight digital inputs, four digital outputs, 
plus some 12 I/O for any possible extension. The IOs are isolated from the MCU.

However, I couldn't get this card to work, almost at all. The Ax58100 didn't even load the eeprom. 
Struggling with the documentation for the Ax58100, which formed my negative opinion about that,
I gave up and let it sit for over half a year. It was first when someone came and asked 
questions around it I had another look. I built a new board, and it all started to work.
At time of writing it all seems to work.
[Link here](Cards/EaserCAT-3000-Digital-Stepper-Analog-Encoder-Frequency/)

## EaserCAT 4000 - THCAD reader

![Picture of EaserCAT 4000 board](Utils/Pictures/EaserCAT-4000.png)

Faced with the problems with the EaserCAT 3000 board, and still wanting to use EtherCAT 
in my upcoming plasma cutter, I made this board whos single purpose is to read 
the PWM frequency from Mesa's THCAD board.\
The THCAD board reads the voltage between 
the plasma torch and the workpiece which is around 100V. Highly isolated, and with 
a voltage to frequency converter it delivers a differential pwm signal. 
The frequency of this signal can be related to the voltage.\
That's what is needed for linuxcnc torch height control.

In the tests on the bench this can read at least up to 200 kHz, but its's better to be below 100 kHz.
The THCAD has a frequency divider one can set to make the frequency range nicely match this.
It seems to work fine so far. 
Implementation is with interrupts. \
A better option would have been to use TIM2's PWM_INPUT function, which can work with several MHz frequency. 
I tried the PWM_INPUT method a lot but couldn't get it to work. The current method still works, though.
If time and interests are in phase with the moon, I might look at PWM_INPUT again.
[Link here](Cards/EaserCAT-4000-THCAD-Reader/)

## EaserCAT 5000 - Digital IO without MCU

![Picture of EaserCAT 5000 board](Utils/Pictures/EaserCAT-5000.png)

A card, just doing Digital Input and Digital Output. The intentions are to read 
limit switches and some other switches around the plasma torch. And to switch on the plasma torch.

It's a single LAN9252, configured for local IO. Optocouplers are attached to IO pins for isolation.
At point of writing it has not been run yet. Still waiting for parts.
[Link here](Cards/EaserCAT-5000-Digital-8In-8Out-LAN9252-only/)

### License

Don't violate the original licenses. No warranties. Use it any way you like.\
Remember: It is a hobby project, not a product.
