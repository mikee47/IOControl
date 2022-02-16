IO Control
==========

Introduction
------------

This library is an asynchronous Input/Output device stack for embedded devices such as the ESP8266.
The main reason for building this was to provide a mechanism for serialising modbus requests.
We also needed to serialise commands to a 433MHz RF switch.

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
  Multiple devices may be attached to a controller.

Request
  Encapsulates a command request for a specific device class.
  Requests are created, queued, executed then destroyed.
  Callbacks are issued to notify the start of execution and completion.

Device Manager
  Keeps track of all registered controllers and provides methods for creating and issuing requests.

Configuration is managed using JSON.
Controller instances are provided by the application.
Devices are defined using a JSON configuration file as devices may be added or removed from a deployed network.

The system for which this library was developed uses a basic HTML/javascript front-end which deals with configuration
and generating command requests (in JSON). This keeps embedded memory requirements to a minimum.

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


API Documentation
-----------------

 .. toctree::
    :glob:
    :maxdepth: 1

    docs/*/index
