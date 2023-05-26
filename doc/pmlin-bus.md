# PMLIN Bus 

## LIN versus CAN

CAN bus requires two more wires and  pins in each connector whereas LIN bus only requires one.

A LIN node only requires a LIN transceiver and an USART, which is often available for 'free' in many applications. Even if we have to add an MCU just for the LIN bus the total cost is around
 one dollar.

A CAN bus cost per node is several dollars because none of the very low cost microcontrollers support the CAN bus and the transceivers and connectors cost more.

* LIN is lower cost (less harness, no license fee, cheap nodes)
* CAN uses twisted shielded dual wires 5V vs LIN single wire 12V
* LIN is deterministic, not event driven 
* LIN clusters have a single master - CAN can have multiple
* CAN uses 11 or 29 bit identifiers vs 6 bit identifiers in LIN
* CAN offers up to 1 Mbit/s vs. LIN at max 20 kbit/s

## PMLIN versus LIN

* PMLIN uses secure CRC for address ID 
* LIN  uses two bit parity for address ID		
* PMLIN PMLIN secure CRC for pay load
* LIN uses simple checksum			
* PMLIN uses positive acknowledge for messages 
* LIN does not acknowledge messages		
* PMLIN has dynamic/automatic address ID assignment 
* LIN expects every slave address to be set 'manually'		
* PMLIN uses 38 400 baud speed 
* LIN typically maxes out 19 200 baud 
* PMLIN supports max 30 devices on a single bus
* PMLIN max bus length is 10 m 
* LIN  max bus length is 40 m
* PMLIN does not use 0x55 SYNC char for baudrate tuning 


# Hardware

At hardware level it is an open collector party line where any connected device, master or slave, can pull the line down.

The idle line state is high or 'pulled up'. Note that this is reverse voltage polarity compared traditional RS232 signalling.

Pulled down line state is called dominant state, because any device pulling the line down will force that state to the line.

Exact electrical specification is not given in this document, ISO 17987-8:2019 standard can be used as guide. 



