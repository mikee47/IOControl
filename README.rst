# IOControl

A simple IO device stack for embedded devices such as the ESP8266. The main reason for building this was to provide a mechanism for serialising modbus requests. We also needed to serialise RF switch
commands as well.

To keep objects generic (i.e. not too modbus-centric) we define the following objects and features:

  * Controller: the hardware which talks over RS485 (or whatever). There is one controller instance for each physical device available. The controller serialises requests.

  * Device: an instance of a modbus slave or RF switch transmitter. These sit on top of a controller to define characteristics specific to the device or slave.

  * Request: Encapsulates a command request for a specific device class.

For modbus devices we have some additional definitions.

  * Address: the address of a modbus slave. Modbus docs. call this the slave ID.
  * Node: Represents something a slave device does. Relay boards have one node for each output it controls.
  * Node ID: The channel a node lives on. For the R421Axx relay boards this is the address or channel number. In a modbus transaction this is the address field.

Nodes aren't actually objects - although perhaps they should be? We could define methods for the nodes (e.g. toggle, etc.) rather than sending commands. The nodes were only introduced in the stack to store meaningful names for each relay output; these are shown on the controller front panel when the corresponding button is pressed. However, the concept of nodes
is very useful so they belong in the structure.

For modbus we define an Controller, Device and Request class. For the relay boards we provide inherited device and request classes to deal with the specifics for these boards. Nodes can be
controlled using generic commands such as 'open', 'close', 'toggle', etc.

Configuration is managed using JSON. Controllers are hard-coded into the platform, but devices are soft-configurable, so we can, for example, define as many R421xx relay boards as we like.

We use a basic HTML/javascript front-end which provides formatting; the content is defined by the config files. We don't make use of external libraries as this has to function without an internet connection. However, a framework such as jQuery, for example, might be useful - the minified version is 35kB.

Hardware topology:

  1. Modbus controller talks to one or more slaves. Initially we have an 8-channel relay slave which can switch and report the state of its circuits. We can enumerate modbus slaves but beyond that not much else without more information. There are some diagnostic modbus commands but whether the cheap boards we're using support them remains to be seen.
UPDATE: Got the command set for the R421A08, which also applies to the 4-channel board (unmarked). In both cases channels from 0 - 15 will elicit a response, but any higher addresses are ignored and produce no response. Enumeration of these boards is possible but in practice we design a system and manually configure it.

  2. RF switch sends commands to RF-enabled light switches. These do not report status.

  3. Other networked devices. We can act as a gateway for these by binding them into the IO control structure.

In software we have a controller base class which we'll use for modbus and rfswitch.

Control functions are mapped using JSON-RPC so can be controlled from any networked device. Primarily this will be a web page served by us, with the control functions via websocket, both on port 80. Therefore anything on the local network can talk to us directly, or with forwarding also via the internet.

Our front panel indicates the state of the 8 relays and provides manual control over them. We shall consider that any additional slaves are only managed via network.

It would be useful to control groups of devices. Our config file will therefore allow groups to be defined.

Controllers are given an ID which indicates their class, plus index within that class:
  * mb0 - Modbus controller #0
  * rf0 - RF Controller #0 (i-Lumos RF Light switch)

Device identifiers are subscripts of their controllers:
  * mb0.1 - device with address 1 on modbus controller 0, indicates the physical connection.
  * rf0.1 - arbitrary numerical ID because they're not required for routing. Switches may respond to multiple codes so using that isn't suitable.

Device, Group and Controller configuration is stored in a JSON configuration file, each with the following common attributes:

  * ID:           Machine identifier, indicates path:
  * Name:         Friendly name to display on web page, etc.
  * Location:     Succinctly indicates where the device/group/controller is
  * Description:  More details about the device.

CModbusController: Hardware comms

CModbusSlave: One of these for each slave device.
  * slaveID
  * baudrate

CModbusDevice: Other types of slave might require different configuration
  * modbusSlave
  * channel

CModbusCommand:
  * id: "on", "off", "toggle"

CRFSwitch: Hardware comms

CRFSwitchDevice:
  * protocol: e.g. i-Lumos
  * flags: DIMMABLE

CRFCommand:
  * id:   "on", "off", "toggle", "up", "down"
  * code

The web page can be generated from the config file or manually created, but IDs must correspond. We can then command a device thus:

  { "ctrl": "mb0", "dev": "1", "cmd": "toggle" }
  { "ctrl": "rf0", "code": 0x123456 }

We can find the appropriate codes by checking in the configuration file, or more easily by just coding them into the HTML.

Command scope may be GLOBAL/CONTROLLER/GROUP/DEVICE. Updated device state is broadcast to all clients. We won't track individual requests; if there's a severe problem then we'd broadcast a status message.

Instead of maintaining a command queue, we just keep an in-memory list of all devices with minimal information: device ID, current state. When a command is received our list is updated with a 'new state'. It's then updated when the controller is available.

Primary functions are:

  1. Report config: Send back the config data (or part of it).
  2. Get current state at global/controller/group/device level. We can include multiple items in a single request. Primary information is ON/OFF/UNKNOWN. We can report multiple items in one request so overall state may include MIXED.
  2. Set state. Only ON/OFF are logical
  3. Send command. For example, dimmable device can have ON: We could emulate this for RF light switches by sending a global OFF followed by an ON. Not very helpful though.
  3. OFF:   Again, for RF stuff not useful.
  4. TOGGLE.

Commands are:

  * Enumerate devices.
  * Query device status. Light switch devices will always return 'unknown'.


# IOControls

23/6/18

Adding support for timers and control (command) chaining. This is for simple automation tasks and only supported within control lists.

We add a CIOControl::oncomplete property which specifies the ID for another CIOControl to invoke. When an IO request completes our requestComplete() method gets called.

Timers are defined by adding the "timer" object to a CIOControl:

    {
      "id": "aux_on",             // When AUX input goes active
      "timer": {
        "id": 0,                  // Identifies a specific timer
        "delay": 1                // Sets the timer, control runs when timer expires
      },
      "device": "rf0",
      "code": "123456",
      "oncomplete": "flash_off"   // Invokes control with ID = "flash_off" when request completes
    },
    {
      "id": "flash_off",
      "timer": {
        "id": 0,
        "delay": 2
      },
      "device": "rf0",
      "code": "7890ab",
      "oncomplete": "aux_on"
    },
    {
      "id": "aux_off",            // When AUX input goes inactive
      "timer": {
        "id": 0
                                  // No duration is specified so timer is left in reset (cancelled) state and never fires
      }
    },

A simpler flashing method:

    {
      "id": "aux_on",             // When AUX input goes active
      "timer": {
        "id": 0,                  // Identifies a specific timer
        "delay": 1             // Sets the timer, control runs when timer expires
      },
      "device": "rf0",
      "code": "123456",           // A toggle code
      "oncomplete": "aux_on"      // Repeats this control
    },
    {
      "id": "aux_off",            // When AUX input goes inactive
      "timer": {
        "id": 0
                                  // No duration is specified so timer is left in reset (cancelled) state and never fires
      }
    },

timer can have:

  * delay:  relative timer, specified in seconds
  * time:   timer fires at a specific time of day, e.g. "14:15", "11:00", "sunset", "sunrise"
  * offset: number of seconds added to time. Normally used with sunset/sunrise.
  * repeat: default is 0 so timer fires only once, specify -1 to repeat indefinitely otherwise repeats a specific number of times.

Mechanism should be flexible enough to support other types of timer (e.g. random).

Sunrise/sunset only needs one timer. We use the 'init' control which gets invoked at startup after the system clock has been correctly set. For example, 30 minutes before sunrise we turn the lights off, 30 minutes after sunset we turn them on:

    {
      "id": "init",
      "oncomplete": "sunrise"
    },
    {
      "id": "sunrise",
      "timer": {
        "id": 1,
        "time": "sunrise",
        "offset": "-1800"
      },
      "device": "rf0",
      "code": "abcdef",         // Lights off
      "oncomplete": "sunset"
    },
    {
      "id": "sunset",
      "timer": {
        "id": 1,
        "time": "sunset",
        "offset": "1800"
      },
      "device": "rf0",
      "code": "123abc",         // Lights on
      "oncomplete": "init"
    },
    {
      // Cancels automatic switching - to re-enable just call 'sunrise'
      "id": "auto_cancel",
      "timer": {
        "id": 1
      }
    }


Using this mechanism a timer can be associated with more than one CIOControl which all participate in related activities. The controls form a chain which can be started and stopped using the timer. However, if a request is queued then the completion callback could just kick things off again, so the control itself must be informed of the cancellation. We therefore need CIOControl::cancel() and this needs to be called for all related controls.
 
  * trigger():        button pressed or input changed
  * execute():        called by trigger() and by when timer fires.
  * onComplete():     when command matching ID is completed
  * cancel():         stop timer if set

Couple of things to watch out for:

  1. Memory leaks. Using CIOControl itself to manage timers means it can only have one timer per object, so a second call will just reschedule the operation; this might happen if we wish to defer the operation, cancel it or submit it immediately (effectively a timeout of 0).

  2. Recursion. Our 'oncomplete' mechanism above could very easily cause recursion. We never call the control directly, but do it through a task queue or timer to prevent deadlocks.

We have an 'oncomplete' mechanism to fire another CIOControl only after all requests in the control have been completed; that takes care of any timing irregularities especially for requests to multiple controllers. To do this we could count submitted/completed requests and trigger onComplete when the counter hits zero.

RTC timers are managed using a single Timer to periodically check them. Short timers (e.g. for flashing) are handled using a separate Timer.

We might consider adding 'at' and 'delay' attributes for an easier and less verbose way to use timers. For example, "delay": "25", "delay": "2:0:0", "at": "dusk + 10:00". 'at' sets up an RTC timer, whereas 'delay' would use a relative timer for perhaps less than 30 seconds but an RTC timer for longer intervals. The timer would be given an appropriate ID, probably from the control ID.

If a request is queued then cancelling the timer won't do anything, and when the request does complete the oncomplete() callback may reinstate it.

We have CIOControl::cancel() then we can check the timer queue for references to that control and cancel them. If the control has queued requests we set a flag, so that when those requests complete the oncomplete() method doesn't fire. This needs to be called for _all_ controls in a chain as we cannot know which is active.

We can use this cancellation technique to disable a control sequence.

At present we pass both CIOControl and command_connection_t to CIODeviceManager::HandleMessage. What we might do instead is create an IOControl instance to wrap up the connection, etc. This will get destroyed when all requests have been completed. We can also use this to defer reporting until all requests in a control have been completed, if that is desirable.

