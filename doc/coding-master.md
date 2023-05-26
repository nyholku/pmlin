# Writing Master Side Code

Using existing devices from the master code is simple.

Here the patient positioning laser ASLAC is used as an example device.

First the relevant header files need to be included. The master code will only need to include the PMLIN master side header and one header file for each Type of device needed. Each slave project should define a header for this purpose.

In this example we use the ASLAC.

```c
#include "pmlin-master.h"
#include "aslac.h"

```

Of course, you need to compile and link the  pmlin-master.c file too. 

Before PMLIN can be used the Hardware Abstraction Layer need to be implemented.


## Hardware Abstraction Layer (HAL)

In order not to tie PMLIN to any specific operating system and hardware all the access to the serial port hardware has been abstracted away to callback functions.

The master code needs to implement these and pass pointers to them to the `PMLIN_initialize_master` function.

Out of the box PMLIN provides implementations for the three functions that make up the HAL. These functions should work in any POSIX compatible OS, so usually there is no need to implement these functions at all.

In addition to the three functions explained below the actual serial port hardware must be initialized in the master code. 

PMLIN does not get involved in setting the baudrate and char size is standardized to because manipulating the baudrate maybe the best way to implement the 'send_break_fun' in a POSIX system. 

So better that PMLIN is not involved in changing the baudrate, see below.

### Read/Write Serial Port functions

The read and write serial port functions are pretty simple, they just take the number of bytes to send and a pointer to a buffer for the data.

The write call can but does not have to be blocking, the read function needs to be blocking and must implement a timeout in case the number of bytes requested does not arrive within the specified time.

The buffers do not need to be valid outside of the functions calls.

The functions must conform to following prototype.

```c
void write_serial_fun(uint8_t *buffer, int bytes_to_send) ;

int read_serial_fun(uint8_t *buffer, int bytes_to_read, int timeout_us);
```

### Send Break function

PMLIN calls the send-break function to cause a longer than eleven bit times long break condition on the serial line.

This is a bit tricky to implement using standard POSIX APIs.

There is the `tcsendbreak()` call but the promise there is that it will cause a break that lasts between 0.25 and 0.5 seconds. Obviously this is not acceptable because it would bring the PMLIN speed way down.

The second best option that POSIX offers is to change the baudrate to half the normal baudrate and send one 0x00 byte. Unfortunately it seems that not every OS/serial driver implements this correctly i.e. changing the baudrate does not honour that some bytes in the OS/driver send queue were queued with different baudrate. For this reason short `usleep()` calls had to be used.

The functions must conform to following prototype.

```c
void send_break_fun();
```


###Initializing PMLIN/HAL


Once the HAL functions have been written PMLIN can be initialized by calling `PMLIN_initialize_master`.

Note that since there are already POSIX compatible versions of these callbacks available it is typically not necessary to write the callbacks at all.

```c
	PMLIN_initialize_master( //
		send_break_fun, //
		write_serial_fun, //
		read_serial_fun //
		);
	....
	}
```
## Defining the network of devices

Next the network of devices needs to be defined. Network here simply means a list of devices connected to the bus defined by their types and IDs.

Suppose we want to use three ASLAC lasers. We need to decide  which  IDs we want to use for those devises, it is most convenient to start from 1 and create  `#define`s  for them.

```c
#define FRANKFORT_LASER_ID 1
#define MID_SAGITTAL_LASER_ID 2
#define LAYER_POSITION_LASER_ID 3
```

Then the network structure is declared as a global array  which is passed to the PMLIN code as follows:

```c
// this at file scope
PMLIN_device_decl_t g_device_defs[] = { //
	ASLAC_DEVICE_DECL(FRANKFORT_LASER_ID), //
	ASLAC_DEVICE_DECL(MID_SAGITTAL_LASER_ID), //
	ASLAC_DEVICE_DECL(LAYER_POSITION_LASER_ID), //
	};

void init_my_PMLIN() {
	...
	PMLIN_DEFINE_DEVICES(g_device_defs);
	}
```
Now we are ready send and receive messages to/from any of the devices in the network, assuming it has been configured correctly i.e. assuming all the actual devices in the network have their correct IDs and are of the correct Type.

It is up to the software architect to decide how and when the actual sending and receiving message takes place.

Before the normal communication over the PMLIN bus starts it should be ensured that
all the actual devices are present and have the correct Types. 

To do that we should call `PMLIN_check_config()` and if it indicates that there is an error in the configuration the system should report that problem to the operator and perhaps instruct him/her to invoke the appropriate service mode.

```c
	int res = PMLIN_check_config();
	if (res != PMLIN_OK)
		handle_problem(res);
```

## Mirroring data between master and slave

As said it is up to the master code author to decide when and how to send/receive messages but the preferred method is to use mirroring.

Mirroring is a process which periodically copies data from masters global variables to the slave and from slave to the masters global variables. In this way the master does not need to explicitly send and receive messages, it is enough to read and write global variables.

From the master code point of view this makes accessing I/O at the slave as simple as accessing other memory mapped hw or I/O.

This method is especially appropriate for the  simple 'on/off' and 'read/write value' kind of I/O that PMLIN is aimed at.

This process ensures that even if a slave goes off line for a period of time it will get up to date information as soon as it comes back online.

To use mirroring two things need to be set up.

First a structure that defines which global variables are associated with which slaves/ messages is set up and passed to PMLIN.

Secondly the `PMLIN_tick()` function needs to be called periodically from a timer interrupt or a background thread. That function takes care of the actual message sending and receiving from the appropriate global variables.

To setup mirroring a global variable defining the mirroring is declared as follows:

```c
volatile uint8_t g_frankforth_laser[ASLAC_CONTROL_MSG_LENGTH];
volatile uint8_t g_mid_sagittal_laser[ASLAC_CONTROL_MSG_LENGTH];
volatile uint8_t g_layer_position_laser[ASLAC_CONTROL_MSG_LENGTH];

PMLIN_mirror_def_t g_mirror_defs[] = { //
		PMLIN_MIRROR_DEF(FRANKFORT_LASER_ID, ASLAC_CONTROL_MSG_TYPE, (void*)&g_frankforth_laser, 10, 0), //
		PMLIN_MIRROR_DEF(MID_SAGITTAL_LASER_ID, ASLAC_CONTROL_MSG_TYPE, (void*)&g_mid_sagittal_laser, 10, 1), //
		PMLIN_MIRROR_DEF(LAYER_POSITION_LASER_ID, ASLAC_CONTROL_MSG_TYPE,(void*) &g_layer_position_laser, 10, 2) //
		};
```

Above sets up mirroring between global variable `g_layer_position_laser` and device with id  `LAYER_POSITION_LASER_ID`
using message type `ASLAC_CONTROL_MSG_TYPE` with a period of 10 ticks at tick number two of that period.

And similarly for g_mid_sagittal_laser and g_layer_position_laser.

## Sending messages manually


It is of course possible to send messages explicitly in code and it is pretty simple.

What needs to be considered is that because it is likely that during the up-time of the master one or more slaves will
reset/reboot and thus the communication should be stateless (not based on previous messages) and periodically repetitive.

Below is an example of how to send and receive messages.

```c
	// send a message
	uint8_t tx_msg[ASLAC_CONTROL_MSG_LENGTH] = { 0 };
	tx_msg[...] = .... // fill the message appropriately
	int res = PMLIN_send_cmd_message( //
		FRANKFORT_LASER_ID, ASLAC_CONTROL_MSG_TYPE, sizeof(tx_msg), tx_msg);
	if (res != PMLIN_OK)
		my_take_appropriate_action(res);

	// receiving a message
	uint8_t rx_msg[ASLAC_STATUS_MSG_LENGTH] = { 0 };
	int res = PMLIN_receice_cmd_message( //
		FRANKFORT_LASER_ID, ASLAC_STATUS_MSG_TYPE, sizeof(rx_msg), rx_msg);
	if (res != PMLIN_OK)
		my_take_appropriate_action(res);
	// process the rx_message[...]
```

In above `ASLAC_CONTROL_MSG_TYPE`, `ASLAC_CONTROL_MSG_LENGTH`, `ASLAC_STATUS_MSG_TYPE` and `ASLAC_STATUS_MSG_LENGTH` are constants for the message types and sizes that the device header (in this example `aslac.h`) should make available for the master.

The master code should always the check the result code of `PMLIN_send_message()` and `PMLIN_receive_message()` and take appropriate action.

Possible actions in case of error include of course error reporting to the operator but also invoking the `PMLIN_check_config()` to ensure that e.g. the service technician has not (by mistake) plugged in a wrong type of slave device which happens to have the ID of matching the ID of a missing device. This could cause unpredictable behavior as the master would then send message formatted for one kind of device to a device of a different kind.





## About Thread safety

PMLIN uses a mutex to prevent concurrent calls from different threads to the PMLIN code in the master to mess up the communication.

In order not to tie PMLIN to a specific operating system the mutex locking is implemented with callbacks passed to PMLIN with `PMLIN_initialize_master(...)`.

If 'NULL' is passed as the mutex object to  `PMLIN_initialize_master(...)` then no mutex locking/unlocking takes place inside PMLIN master code.

The mutex lock/unlock callbacks are compatible with `pthread` mutex calls for ease of use.

Following calls lock the mutex for the duration of the call:

```c
	int PMLIN_send_message(...);
	int PMLIN_receive_message(...);
	int PMLIN_send_cmd_message(...);
```

Following two calls lock the mutex for the duration of the whole procedure they perform which can take several seconds:

```c
	int PMLIN_auto_config(...);
	int PMLIN_check_config(...);
```

Following call locks the mutex for the duration of each of the `PMLIN_send_message` or `PMLIN_receive_message` calls that it makes.

```c
	void PMLIN_mirror_ticks(...);
```
Other functions in PMLIN that are used to setup and initialize PMLIN are not thread safe and are intended to be called once at boot time.

PMLIN code (slave and master) have been written with some assumptions that are NOT guaranteed by the C standard but which nevertheless are reasonable expectations and in most environments true.

Both the slave and master code have been written with the following assumptions:

* access to 'volatile uint8_t' is atomic
* access to 'volatile' variables are not re-ordered by the compiler
* changes to 'volatile' variables are visible to all threads

The first one, (assignment to 'volatile uint8_t' is atomic) is almost bound to be true as most processors use byte as the single addressable unit and use a single cycle to read or write that memory. However this is not guaranteed by the C standard.

The second one (access to 'volatile' variables are not re-ordered by the compiler) this is actually guaranteed by the standard AFAIU.

The third one (changes to 'volatile' variables are visible to all threads) is often true because most processors implement caching transparently so a memory write, even if cached, is visible to other threads reading it.

Some of the above assumptions may not be valid in a given environment, especially in multicore and even more so in multiprocessor environments with shared memory.

Note that the use of a mutex should guarantee memory synchronization even in those environments.
