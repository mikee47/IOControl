IOControl
=========

Introduction
------------

An IO device stack for embedded devices such as the ESP8266.
The main reason for building this was to provide a mechanism for serialising modbus requests.
We also needed to serialise RF switch commands as well.

To keep objects generic (i.e. not too modbus-centric) we define the following objects and features:

Controller
  The hardware which talks over RS485 (or whatever).
  There is one controller instance for each physical device available.
  The controller serialises requests.

Device
  An instance of a modbus slave or RF switch transmitter.
  These sit on top of a controller to define characteristics specific to the device (or slave).

Request
  Encapsulates a command request for a specific device class.

For modbus devices we have some additional definitions.

Address
  The address of a modbus slave. Modbus docs. call this the slave ID.
Node
  Represents something a slave device does. Relay boards have one node for each output it controls.
Node ID
  The channel a node lives on. For the R421Axx relay boards this is the address or channel number.
  In a modbus transaction this is the address field.

For efficiency, nodes aren't implemented as objects, just identifiers used by a Device / Controller.
The nodes were only introduced in the stack to store meaningful names for each relay output;
these are shown on the controller front panel when the corresponding button is pressed.
However, the concept of nodes is very useful so they belong in the structure.

For modbus we define an Controller, Device and Request class.
For the relay boards we provide inherited device and request classes to deal with the specifics for these boards.
Nodes can be controlled using generic commands such as 'open', 'close', 'toggle', etc.

Configuration is managed using JSON. Controllers are hard-coded into the platform, but devices are soft-configurable.
We can, for example, define as many R421xx relay boards as we like.

The system for which this library was developed uses a basic HTML/javascript front-end which deals with configuration
and generating command requests (in JSON). This keeps embedded memory requirements to a minimum.

Hardware topology
-----------------

1. Modbus controller talks to one or more slaves.
   Initially we have an 8-channel relay slave which can switch and report the state of its circuits.
   We can enumerate modbus slaves but beyond that not much else without more information.
   There are some diagnostic modbus commands but whether the cheap boards we're using support them remains to be seen.

   UPDATE: Got the command set for the R421A08, which also applies to the 4-channel board (unmarked).
   In both cases channels from 0 - 15 will elicit a response, but any higher addresses are ignored and produce no response.
   Enumeration of these boards is possible but in practice we design a system and manually configure it.

2. RF switch sends commands to RF-enabled light switches. These do not report status.

3. Other networked devices. We can act as a gateway for these by binding them into the IO control structure.

Controllers are given an ID which indicates their class, plus index within that class:
- ``rs485#0`` RS485 serial controller #0
- ``rfswitch#0`` RF Controller #0 (i-Lumos RF Light switch)

Device identifiers can be anything but must be globally unique, e.g. `mb1`, `rf0`, `dmx1`, etc.
See :sample:`Basic_RS485` sample, config/devices.json.

We can command a device thus:

.. code-block:: json

  { "device": "mb1", "node": 1, "command": "toggle" }
  { "device": "rf0", "code": 0x123456 }

Commands are defined using a command set of codes via :cpp:enum:`IO::Command`.
Not all device types support all commands.
