
# PMLIN Protocol

The PMLIN protocol is very similar to the LIN network protocol but instead of simple checksum it uses proper 8 bit CRC to ensure error detection.


## Single Master Controls Multiple Slaves

The basic principle is one master multiple slaves where the master always transmits first and only the addressed slave can transmit if so instructed by the master.

## Unique ID for Each Slave

Every slave has an ID address which the slave stores into its non-volatile memory. The ID can be changed by the master via a RENUM message.

Every device in a network needs to have a unique address.

## Each Slave has a Type

Every device has a type associated assigned to it.

## Standard 8-bit Async + BREAK

The lowest level protocol is standard async, with a start bit, 8 data bits and a stop bit at 38.400 baud speed.

Every packet/message starts with a BREAK condition which is defined as 'pulled down' or dominant line state that lasts longer than 11 single bit times.

## Throughput

The actual throughput of the network depends on the actual messages but given the minimum payload of one byte plus CRC, two byte header and an acknowledge byte the minimal message length is about 1.4 msec plus the break at 0.6 msec, so the theoretical minimal message length is about 2.0 msec or 500 messages/sec.

Note that this is purely a theoretical limit value imposed by the protocol, not something that should be attempted in practice.

## Packet Structure

Visual Representation of the PMLIN Protocol

![Alt text](pmlin-protocol-v1.0.png?raw=true "Title")

### Header
 
Each packet starts with a BREAK condition. The length of this is not defined but an easy way to generate this in a PC or Linux master is to send a 0x00 character at half the normal baudrate (so at 19.200 baud). This will create an approximately 0.6 msec break condition.

After the BREAK the master always sends a two byte header that contains the target slave ID, message type and a CRC checksum for the header.

### Payload
After the header either the master or slave transmits the payload. The direction (M->S or S->M) and length of the payload depends on the type of the message. The protocol imposes no limit on payload length.

The last byte the payload is an 8 bit CRC computed over the other bytes of the payload.

For master to slave payloads the slave acknowledged a correctly received payload by sending an 'ACK' character 0x55.

### Command messages

No slave can have the ID value 0 (zero) so this is reserved for broadcast messages that all slaves should receive, decode and act accordingly. So far no broadcast messages have been specified but one use for broadcast that is envisioned is synchronising slaves at/to exact moments in time.

For other IDs (1-30) the message type defines the payload length, direction and meaning and PMLIN protocol does not dictate any specific structure or length for the payload part of the message.

All this is something that both the master and slave just 'need to know'. In practice this is communicated via header files at code compile time.

The type 7 message is reserved for the protocol housekeeping and indicates a special command message which, unlike any other message, transfers a five byte payload to both directions, first five bytes are from master to slave and the next five bytes from slave to master. No other type of message can transfer data back and forth between a slave and the master.

The first byte of command message payload encodes the type of command that the master sends to the slave as follows:

0x00 PROBE to check slave presence in then network and check ID conflict

0x01 RENUM to re-assign a slave ID (change the ID of a slave)

0x02 INQUIRE to inquire the type of a slave and get its firmware version


## Solving ID conflicts with PROBE and RENUM

After manufacturing each device of any given type will have the same ID which causes a conflict if multiple devices of the same type are connected to the bus. 

The master solves this problem by probing each ID with a PROBE message to find out if there is a conflict for a given ID and then uses the RENUM message to solve the conflict.

Both the PROBE and RENUM message payloads (from slave to master) have some random content that guarantees that the message will exhibit a CRC error if two slaves respond to the message.

This ensures that a PROBE message fails if there are conflicting IDs and the failure indicates the conflict.

When the master sends a RENUM message the slave waits a random time before responding in order to avoid a competing slaves response message.

When responding to the RENUM the slave monitors its own communication and if and only if it receives its own message back intact will it then actually re-assign its own ID.

This ensures that only one slave will actually change its ID for any given RENUM attempt.

Those slaves that are still waiting for their own (random) time slot also monitor the bus traffic and abort their renumbering effort as soon as they notice that an other slave has started to transmit anything.

## Device Type, Firmware and Hardware Revision inquiry

In addition to an ID every slave has a type code that declares what kind of device it is and a firmware version number. The master can interrogate that information with the INQUIRE message.

The hardware revision number is also included in the inquiry response.

## Resolving configuration issues with the INQUIRE message

If all the devices on the PMLIN bus have unique types then PMLIN master can automatically, on request, assign unique and correct ID for each device using the `PMLIN_auto_config` function.

If, on the other hand, the system has a number of devices with same ID and type then the master needs to provide some kind of user interface (often called service mode) in which a service technician can assign proper ID for all devices of the same type.

To do that, the master will first assign unique ID to each.

Then for each device of same type the master will display to the technician a message like "Please press ID button on the 'left side fudget'" and then by reading the button states of each device using INQUIRE messages the master can find out which 'fudget' was indicated. 

In this way the master can find out if the IDs are correctly assigned and if not it can correct the assignments with RENUM messages.

An alternative approach is for the master to display to the technician a list of devices and 'visually indicate' each device in turn to the technician with the INQUIRE message and request the technician to select from the list which device was indicated. 

A 'visual indication' can be e.g. a dedicated LED on the device, or it can utilize the primary function of the device, for example by blinking the laser in case of a laser light device.

The protocol simply provides a standard way to activate the indication and read the button.
