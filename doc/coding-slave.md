# Implementing Slave Devices

The slave side PMLIN code has been written with the assumption that it runs on 'bare metal' without an operating system and that the MCU is fairly limited.

The example slave device which can be used as a reference implementation runs on ATTiny1616 processors which is an 8 bit processor and the code uses less than 2 kB of Flash and less than 100 bytes of RAM. This is a fairly cheap processor, at the time of writing this the low volume prising was $0.86

To keep the PMLIN slave side code hardware independent all of the hardware related functionality has been abstracted away to callbacks.

To conserve resources and to allow the C compiler to effectively optimize the code the PMLIN slave side code does not use function pointers to implement callbacks, instead the `pmlin-slave.h` file declares the prototypes for the necessary functions and the implementer of the actual slave code needs to provide the actual function definitions in the device code.

 Most of these functions that you need to implement are very short and simple, almost oneliners.


The callbacks can be categorized to three classes to help comprehension.

# Initializing PMLIN slave 

## PMLIN_initialize
```c
// call this to initialize PMLIN code and set the slave id
void PMLIN_initialize(uint8_t id, uint16_t device_type, uint16_t firmware_version);
```
Before calling any of the other PMLIN routines the library needs to be initialized by calling `PMLIN_initialize`.

This function takes as arguments the device address ID, which should be retrieved from nonvolatile storage, the device type which should be defined in a device type specific header to be shared with master code and the firmware version number of the device.


# Implementing callbacks to access the message payload data

The first category of callbacks is used to deliver and fetch the actual payload data to/from the client code.

## PMLIN_init_transfer
```c
// implement this, PMLIN code calls this to allow the client code to prepare for data transfer based on the message type)
uint8_t PMLIN_init_transfer(uint8_t type);
```
After having received the packet/message header PMLIN calls this function to allow the client code to indicate to PMLIN what to do with the specified `type` of message. The function needs to return  one of the following constants and do what ever it needs to do to prepare to send or receive the payload.

```c
PMLIN_INIT_IGNORE_MSG
PMLIN_INIT_RX_MSG
PMLIN_INIT_TX_MSG
```

Typically `PMLIN_init_transfer`  zeroes an index to the payload buffer and `PMLIN_handle_byte_received_from_host` stores the received byte to that index and increments the index.

PMLIN uses internally a small buffer (8 bytes) for its own data transmission/reception and an index and message length variable for that buffer.

```c
extern volatile uint8_t g_PMLIN_buffer[PMLIN_BUFFER_SIZE];
extern volatile uint8_t g_PMLIN_trf_idx;
extern volatile uint8_t g_PMLIN_trf_len;
```
These are also available for the client code if the client can manage with such a small buffer. If the 'g_PMLIN_buffer' is utilized then it is important that the data to be sent is be written to the buffer during `PMLIN_init_transfer` and read out during `PMLIN_end_transfer`.

If a separate buffer is used then the client code can keep the data in the buffer all the time.

## PMLIN_end_transfer
```c
// implement this, PMLIN code call this when the transfer is complete to allow the client to process the received message
void PMLIN_end_transfer(uint8_t type);
```
After the payload has been sent or received PMLIN informs the client code about the end of transfer by calling this function. This function is not called if there is CRD error during receptions.

 For convenience the same `type` argument that was passed to `PMLIN_init_transfer` is also passed to this function.


## PMLIN_handle_byte_received_from_host

```
// implement this, PMLIN code calls this from within the data received interrupt to deliver the payload to the client code
bool PMLIN_handle_byte_received_from_host(uint8_t byte);
```

For messages from master to slave PMLIN calls this function to deliver the actual data to the client code byte by byte.

It is worth noting that PMLIN poses no limit on the length of the payload, the message type implies the size of the payload and it is up to the client code at both master and slave to manage this as they please.

This function should return `true` if this was not last byte of the message.

## PMLIN_get_byte_to_transmit_to_host
```
// implement this, PMLIN code calls this to get next byte to send, return -1 when there are no more bytes to send
int16_t PMLIN_get_byte_to_transmit_to_host();
```
For messages from the slave to master PMLIN calls this function to get the payload data byte by byte.

As PMLIN imposes no limit to the length of the payload and does not manage the payload length. It is up to the slave client code to manage this. When there are no more bytes to send this function must return -1 to indicate end of message to PMLIN.

To make this possible the function return type is `int16_t` and not `uint8_t` although the actual payload data is bytes of course.


# Implementing callbacks to the serial port hardware


The seconf category of callbacks exists to access the serial port hardware.

The underlaying programming model is an interrupt driven UART type hardware.

The client code is expected to provide two interrupt handlers for the UART hardware which generates and interrupt when a byte has been received or more data can be sent (because previous bytes have been sent).

The client code  interrupt routines need to call the appropriate interrupt handler in PMLIN code.

Interrupts must not be nested.

## PMLIN_UART_data_register_empty_interrupt_handler
```c
// this needs to be called when the DRE interrupt has been enabled and the UART is ready to receive next char
uint8_t PMLIN_UART_data_register_empty_interrupt_handler();
```
This is a function provided by the PMLIN that the client interrupt handler needs to call from within its own interrupt handler when the cause of the interrupt is Data Register Empty (DTE) indicating that more data can be sent.

The interrupt handler should transmit the byte returned from PMLIN_UART_data_register_empty_interrupt_handler by  e.g. by writing the returned value to the actual UART transmit data register.

This function *always* returns the next character to be sent through the serial port and therefore it is critical that if PMLIN calls `PMLIN_UART_enable_data_register_empty_interrupt(false)` from within this function that no further DRE interrupts are generated after the byte returned from that call to `PMLIN_UART_data_register_empty_interrupt_handler` has been sent *and* that after PMLIN code calls `PMLIN_UART_enable_data_register_empty_interrupt(true)`  a DRE interrupt is immediately generated to allow PMLIN to deliver the first byte to be sent.

## PMLIN_UART_data_received_interrupt_handler
```c
// this needs to be called when a new character has been received
// note that this must not be called for the break char but if the break has been detected then the 'break_detected'
// needs to be true when the first character of the header is delivered to PMLIN in the 'data_in'.
void PMLIN_UART_data_received_interrupt_handler(uint8_t data_in, bool break_detected);
```
This is a function provided by the PMLIN that the client interrupt handler needs to call from within its own interrupt handler when the cause of the interrupt is Receive Complete (RXC) indicating that a new byte has been received.

In addition the the received byte `data_in` the client interrupt handler needs to ensure `break_detected == true` if and only if the `data_in` is the first character received after the last break condition on the serial line. Break condition alone should not result in a call to the this function.

#### PMLIN_UART_enable_data_register_empty_interrupt

```c
// implement this, PMLIN code calls this to control the transmit data register empty UART interrupt
void PMLIN_UART_enable_data_register_empty_interrupt(char enable_interrupt);
```
PMLIN code calls this function to enable or disable hardware interrupts for the DRE condition.  PMLIN typically calls this with the `enable` set to `true` when it wants to start sending data through the serial port.

PMLIN calls `PMLIN_UART_enable_data_register_empty_interrupt` function with the `enable` set to `false` from within `PMLIN_UART_data_register_empty_interrupt_handler()` when it is about to return the last character to be sent (see description of that function) after which it expects not to receive any more calls to the DRE interrupt handler.






### Implementing callbacks for misc services



The thrid category of callbacks exists so that PMLIN can get access to services usually provided by the operating system, such as persistent storage, timing and random number generation.

#### PMLIN_random
```c
// implement this, PMLIN code calls this get a random number in the range 0..65535
uint16_t PMLIN_random();
```
In several places in the PMLIN protocol random numbers are needed either as message content or for deciding on a random response time to avoid collisions. PMLIN calls this function to obtain a random number. This should be a 'true' random number and not a pseudo random number. A pseudo random number might cause two or more  slaves of the same Type to lock in on a deadly sync.

The 'quality' of the returned random number does not need to very high, for example a fast (CPU clock) rate timer value would  in all likelihood be random enough because of long term clock drift and varying reset signal rise times.

#### PMLIN_store_id
```c
// implement this, PMLIN code calls this to store the id into EEPROM when the slave is renumbered
void PMLIN_store_id(uint8_t id);
```

The slave devices need to retain their ID addresses until commanded by the master to re-assign themselves.

When that happens PMLIN calls this function with the new ID and this function needs to store the ID into
a nonvolatile memory, such as EEPROM.

There is no corresponding ´retrieve_id´ function to emphasize the static nature of the ID, instead the
ID retrieved from the nonvolatile memory when the slave boots is passed to PMLIN in the call to `PMLIN_initialize(...)`.

#### PMLIN_TIMER_interrupt_handler
```c
// this needs to be called from a regular system tick interrupt
void PMLIN_TIMER_interrupt_handler();
```

PMLIN needs to time certain operations in the protocol. For this purpose it needs to have a regular tick or heartbeat.

The client is expected to provide or have some hardware interrupt based timer service routine available for this purpose.
That timer interrupt routine must call `PMLIN_TIMER_interrupt_handler` on every interrupt.

The accuracy and precision of this heartbeat (jitter and latency included) is specified here to be better than 1 msec.

#### PMLIN_set_timer_period
```c
// call this to initialize PMLIN code and set the slave id
void PMLIN_set_timer_period(uint16_t period_in_usec);
```
PMLIN does not specify what the timer interrupt period is, rather the client code needs to tell PMLIN what it is by calling this function.

No hard bounds for the period_in_usec have been determined, but a timer period of 500 usec (ie period_in_usec == 500) is recommended.
