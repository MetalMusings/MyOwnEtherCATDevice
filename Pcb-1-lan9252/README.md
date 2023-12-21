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

The .ioc file can be opened in CubeMX. The STM23F407 processor has functions tied to specific pins, the .ioc file has this info. This is just for reference.