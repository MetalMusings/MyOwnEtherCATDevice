# Making my own EtherCAT device

This repository contains software, pcb shematics and layouts, reference documentation etc
that I used during the creation of my own EtherCAT device.

This job is documented in a series of Youtube videos, from my first attempts to understand
how EtherCAT works, to making my own pcb, program it and testing it in LinuxCNC.

## Make my own EtherCAT device 9. About time. Step generator

This was something, I almost gave up. But here it is. A working step generator.
Actually, not one but two step generators for the EaserCAT 2000 board.
For a step generator it is all about timing, timing and timing.
I'll walk you through what I had to do to get it to work.
It can give some general insight into how EtherCAT for linuxcnc works.

Biggest changes this time are in the [Firmware folder](Firmware).
The [Documentation folder](Documentation) has design details on the second step generator.
A small but fatal bug was fixed in the EEPROM_generator.
For all details, read the git log. Too much to mention all.

[![Watch the video](https://img.youtube.com/vi/boanv6ihYtI/default.jpg)](https://youtu.be/boanv6ihYtI)

## Make my own EtherCAT device 8. EaserCAT 3000

Introducing the new **EaserCAT 3000** board. It's an evolution of the EaserCAT 2000
and intended to be used in my plasma cutter. It features four stepper driver outputs,
input for a THCAD arc voltage card, an encoder, an analog output (for spindle +- 10V),
eigth digital inputs, four digital outputs, plus some 12 I/O for any possible extension.

The KiCAD files are in the [KiCAD folder](Kicad/Ax58100-stm32-ethercat)

[![Watch the video](https://img.youtube.com/vi/boanv6ihYtI/default.jpg)](https://youtu.be/boanv6ihYtI)


## Make my own EtherCAT device 7. Turning in the lathe

I have now put things together so the EaserCAT 2000 card controls
my small CNC lathe. Two stepper generators, one each for the X and Z axes,
and an encoder counter for the spindle encoder are on this small card.

While it works there are still issues caused by the variation in cycle time. Thankfully
I was able to reduce the variation from 80-100 microseconds down to 2-3 microseconds,

[![Watch the video](https://img.youtube.com/vi/Bqi1KXEVI1Q/default.jpg)](https://youtu.be/Bqi1KXEVI1Q)

## Make my own EtherCAT device 6. Stepper motor driver

The stepper driver generator shows up and makes some stepper motor sounds. The two big things
this time have been setup of the timer for the stepper pulses and synchronizing the EtherCAT
cycle with the linuxcnc servo-thread cycle. Thankfully I don't show much of that.

[![Watch the video](https://img.youtube.com/vi/QNNEA0wO4Mw/default.jpg)](https://youtu.be/QNNEA0wO4Mw)

## Make my own EtherCAT device 5. The lathe is alive

I hook up the EaserCAT 2000 board to my mini-lathe and make it work.
Documentation is available here, please select the *Video5* branch.

[![Watch the video](https://img.youtube.com/vi/wOtMrlHCCic/default.jpg)](https://youtu.be/wOtMrlHCCic)

## Make my own EtherCAT device 4. The PCB is here

In this video it starts to be interesting. I have got the pcb and I try to make it work.
Now I finally make documentation available, see [this folder](Pcb-1-lan9252).

[![Watch the video](https://img.youtube.com/vi/An0VrKYAv88/default.jpg)](https://youtu.be/An0VrKYAv88)

## Make my own EtherCAT device 3. Encoder

I made a rudimentary EtherCAT encoder module. To test it I need something better than my test setup so I start to design my own PCB to be used for testing with LinuxCNC.

[![Watch the video](https://img.youtube.com/vi/oNIBOpeTpQ4/default.jpg)](https://youtu.be/oNIBOpeTpQ4)

## Make my own EtherCAT device. 2. MCU and SPI

Testing the SPI connection between the MCU and the LAN9252 chip. Get into state diagrams for EtherCAT and for CIA402.

[![Watch the video](https://img.youtube.com/vi/F9HdCEG6kow/default.jpg)](https://youtu.be/F9HdCEG6kow)

## Make my own EtherCAT device. 1 Digital IO

My first tries with the LAN9252 chip. Getting accustomed to some tools.

[![Watch the video](https://img.youtube.com/vi/IGmXsXSSA4s/default.jpg)](https://youtu.be/IGmXsXSSA4s)



# EtherCAT PCB based on LAN9252 and STM34F407

##### The firmware is in the **Firmware** folder.

Open this in PlatformIO. PlatformIO might be able to install all necessary software. If not, you need at least Arduino for STM32 (STM32duino) and SPI. PlatformIO might be able to figure that out, I hope so.
SPI settings are in lib/soes/hal/spi.cpp if you are interested.
SOES is the Arduino port of SOES from <https://github.com/kubabuda/ecat_servo/tree/main/examples/SOES_arduino>
My own code is in main.cpp. The encoder counter code is in STM34_Encoder.cpp

###### The PCB design is in **Kicad**.

Open this with Kicad. I have put the symbols and footprints in there, just set the right paths.
The design is in the state I ordered the pcb from Aisler, Germany.

###### The **Schematics** folder

contains reference documentation, schematics of several LAN9252 evaluation boards and other boards. This is from where I got inspiration and ideas to the design. No I didn't read all the reference documentation - I copied most from these designs and that's why it also worked the first time. [Freerouting](https://github.com/freerouting/freerouting) was used to route the pcb. Normally I route the nets by hand but this time it was a bit much and it came out quite nice and, most importantly, it worked. 

##### ESI xml file generator is in **EEPROM_generator**.

Access the folder with a file browser and click on index.html to make or update your own xml file (and other necessary files, incl eeprom bin). Copy the contents to Firmware/lib/soes
and to Twincat esi xml file directory `c:\twincat\3.1\io\modules\ethercat`
The original EEPROM_generator is found here <https://github.com/kubabuda/EEPROM_generator>

##### **CubeMX-files** are for reference only.

The .ioc file can be opened in CubeMX. The STM32F407 processor has functions tied to specific pins, the .ioc file has this info. This is just for reference.

##### ESI (EtherCAT Slave Information) files

I have put [Dig_8IN_8OUT.xml](Dig_8IN_8OUT.xml) here, this is an example ESI file for the LAN9252 IC standalone, no MCU needed. It's sometimes handy to have the simplest possible ESI file at ahnd, here it is. You find the ESI file for the Encoder application [here](Firmware/lib/soes/MetalMusings_EaserCAT_2000_encoder.xml)

##### **linuxcnc** contains modifications to make EaserCAT 2000 work

The configuration files and the hal component I used are placed here.

##### LAN9252_eeprom_store_valid

Arduino sketch to program the AT24C32 eeprom with a valid eeprom content. Hook up the eeprom using I2C and run the program. The verification in the of theprogram must be passed for valid programming.



###License

Don't violate the original licenses. No warranties. Use it any way you like.
