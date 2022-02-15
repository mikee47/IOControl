IOControl
=========

Introduction
------------

This library is an Input/Output device stack for embedded devices such as the ESP8266.
The main reason for building this was to provide a mechanism for serialising modbus requests.
We also needed to serialise RF switch commands as well.

The goal is to abstract away differences between different communication protocols and provide
a uniform API for controlling a network of device **nodes**.

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

For modbus devices we have some additional definitions:

Address
  The address of a modbus slave. Modbus docs. call this the *slave ID*.
Node
  Represents something a slave device does. Modbus relay boards have one node for each output it controls.
Node ID
  The channel a node lives on. For the R421Axx relay boards this is the address or channel number.
  In a modbus transaction this is the address field.

For efficiency, nodes aren't implemented as objects, just identifiers used by a Device / Controller.
Nodes can be controlled using generic commands such as 'open', 'close', 'toggle', etc.

Configuration is managed using JSON. Controllers are hard-coded into the platform, but devices are soft-configurable.
This means only JSON configuration needs to be updated when new devices are added or removed from the network.

The system for which this library was developed uses a basic HTML/javascript front-end which deals with configuration
and generating command requests (in JSON). This keeps embedded memory requirements to a minimum.
Additional configuration files are provided for use by the front-end to further describe each device,
including a list of node IDs with user-friendly names.


Hardware topology
-----------------

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
