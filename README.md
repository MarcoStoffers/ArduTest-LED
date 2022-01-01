# ArduTest-LED
![ledtest_front](https://marcostoffers.github.io/ledtest_front.jpg)

Based on an idea of https://jaycar.com.au

## Update: new Firmware V0.6 with adjustable LED PWM start voltage

The PWM start voltage for testing the LED was fixed set to 2.5V. This was good for standard LEDs. But if you want to test white or blue LEDs with 3V or higher, it won't work. So I added a setup for this startup voltage. Press "I up" ("Strom +") while switching on the tester and wait for the startup screen to complete.

![lcdvstartsetup](https://marcostoffers.github.io/lcdvstartsetup.jpg)

Now you can adjust the LED voltage with the "V up" ("Spannung +") and "V down" ("Spannung -") keys. IF YOU SET THIS VOLTAGE TO HIGH, YOUR LED WILL BLOW UP! For storing this value, press "I down" ("Strom -") key.

If this is the first power up for new firmware version, the default value of 2.5V will be stored in EEprom. You will see a short notice on the LCD

![eeprominitok](https://marcostoffers.github.io/eeprominitok.jpg)

## An Arduino based LED Tester with LCD

Select the current and the destination voltage (your battery), connect the LED to the tester and get the needed value for a serial connected Resistor. You can also see the measured values for the LED (forward voltage and current).

To dimm the LED, you can lower the current during measurement down to 3mA or up to 40mA. The Tester always starts with a default current of 10mA!

## Tips for DIYing

The housing is designed in Fusion360. You can print it on any 3D FDM Printer with PLA or any other material you want. You will need 4x threaded inserts in M3 and 4 screws M3x25 to hold the front on the housing. The Frontplate is desgined in Inkscape in german. You can edit the SVG file for your own like.

The buttons are also designed in Fusion360. You will need an SLA Printer to get good results (FDM Printer will not work here fine).

## EasyEDA Source for PCB ordering through JLCPCB

https://oshwlab.com/mstoffers/ArduTest-LED

## License

![CreativeCommonLicense](https://marcostoffers.github.io/cc.png)
