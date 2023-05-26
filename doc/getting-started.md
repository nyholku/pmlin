 
# Getting Started

## Clone the Repo
The easiest way to get started is to clone the repo and compile master-demo program.

```console
cd ~
git clone https://github.com/nyholku/pmlin.git .
```

## Compile and Run the Master Demo

Assuming your basic C-tool chain is installed and standard  you can compile the master-demo with make:

```console
cd pmlin/master-demo
make
```

To run the demo without hardware execute:

```console
./pmlin-demo -e 0
```

This starts the demo with three emulated PMLIN slave devices and
displays a list of keyboard keys you can use to exercise 
PMLIN functionality. Note that the commands are executed immediately without waiting for the ENTER key.

```console
./pmlin-demo -e 0
Starting 3 emulated slave devices
Following (keyboard keys) commands are available:
 0..9 : set target device id
 t    : toggle target output on/off
 s    : read target status message
 i    : inquire target device type, fw and hw versions
 p    : probe the target device
 n    : renumber prev target id to current target id
```

## Compile and Run the Slave Demo


To run the master-demo with a real device you need  you need real hardware:

* PC with serial port
* ATtiny 3217 Xplained Pro evaluation board
* Wiring, see here for [schematics for experimenting](schematics.png)

To compile the slave code you need to have Microchip MPLAB X with XC8 compiler /linker with ATTinyDFP device support installed.

Note that the git repo does not include (as per Microchip best practice advice) the Makefiles, to generate those you need to open the project at least once with MPLAB X.

Then you should be able to compile the slave demo code by with:

```console
cd ~/pmlin/slave-demo
make
```

Or you can compile it in the MPLAB X IDE which is probably the easiest path as you need to Flash the code into the development kit and doing that from the command line is not totally trivial.

To compile the code in MPLAB X IDE you need to open the project from `pmlin\slave-demo` and selecte from the **Production**  menu the **Make and Programg Device Main Program** command.

Once that code has been flashed a LED on the evaluation kit should start blinking at mostly-off pulse rate.

You can then talk to the slave with master demo like this:

```console
cd ~/pmlin/master-demo
./pmlin-demo /dev/serialport 0
```

### Inquire Slave Firmware etc

You can inquire the slave firmware version etc with **'i'** key.

```console
> Inquire device id 1
dev type 2 firmware ver 1.2.3 hardware rev 0 button=OFF
Done.
```
### Toggle Slave LED

You can make the evaluation kit LED toggle between mostly-on
and mostly-off blink modes by typing the toggle command **'t'**.

### Re-assign Slave ID

Out of the box after flashing the demo slave has ID 1.

To assign it for example ID=7 you can type  **'17n'**.

```console
> target ID set to 1
target ID set to 7
Renum device id 1 to id 7
Done.
```

The master-demo only support IDs 1-9 to keep the UI sleek.
