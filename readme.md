# Introduction


This repo  contains code and examples to implement both master and slave nodes for a PMLIN bus devices.

PMLIN is a cheap and cheerful standard for interfacing simple I/O devices to a more expensive main controller.

PMLIN is used to control for example patient positioning lasers and mood lights in a new Cone Beam CT scanner, see below.

![](xray.jpg)

PMLIN builds on the LIN bus concept.

LIN bus is used for example in cars in places where CAN bus is an overkill in terms of cost and performance. 

LIN and CAN buses serve the same purpose but fulfill different niches. 

Both are used to simplify and reduce wiring and to enable modularity. 

PMLIN is implemented in ISO 99 C and attempts to comply with MISRA C guidelines.

PMLIN is licensed under the permissive [3-Clause BSD License](license.txt).

# Repo / Code Organization

To keep things simple this repo contains all the code and documentation necessary to compile and run both the examples code and to implement an actual PMLIN master and slave nodes.

This means that some of the code is 'dead' for any given application but since most of the code is one or two small files this has little consequenses. 

[includes](includes) folder contains headers that both master and slave code needs to include in their C-code.

[master](master) folder contains code necessary to implement a master controller.

[slave](slave) folder contains code necessary to  implement a slave node.

[master-demo](master-demo) folder contains a  command line demo program that can be used to demonstrate most PMLIN functionality in a PC. 

[slave-demo](slave-demo) folder contains a full implementation of a minimal PMLIN slave node ready to be compiled with MPLAB X and to run on a ATtiny 3217 Xplained Pro development board.

[doc](doc) folder contains all the documentation.


# Further Reading / Getting Started

This is a comprehensive list to the documentation in the [doc](doc) folder.


* [Getting Started](doc/getting-started.md)

* [PMLIN Bus](doc/pmlin-bus.md) 

* [PMLIN Protocol](doc/pmlin-protocol.md) 

* [Coding Master](doc/coding-master.md) 

* [Coding Slave ](doc/coding-slave.md) 