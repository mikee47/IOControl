R421A Relay Boards
==================

.. image:: r421a08.png

These are inexpensive and readily available MODBUS multi-channel relay boards.
Both 8 and 4-channel versions have been tested.

Device properties include:

Address
  The address of a modbus slave. Modbus docs. call this the *slave ID*.
Node
  Represents something a slave device does. Modbus relay boards have one node for each output it controls.
Node ID
  The channel a node lives on. For the R421Axx relay boards this is the address or channel number.
  In a modbus transaction this is the address field.

8-channel
  - DIP switches set the slave address, from 0x00 to 0x3f. e.g. A0 ON=#1, A1 ON=#2, etc.

4-channel
  - Set A5 ON for MODBUS RTU mode (OFF is for AT mode).
  - Set A0-A4 to slave address, from 0x00 to 0x1f.


API
---

.. doxygennamespace:: IO::Modbus::R421A
   :members:
