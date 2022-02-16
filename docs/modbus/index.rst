MODBUS
======

https://en.wikipedia.org/wiki/Modbus

Implements MODBUS-RTU protocols.

Can use this directly, but generally preferable to create custom Device and Request classes.
See :doc:`../r421a/index` for an example.

Device properties include:

Address
  The address of a modbus slave. Modbus docs. call this the *slave ID*.
Node
  Represents something a slave device does. Modbus relay boards have one node for each output it controls.
Node ID
  The channel a node lives on. For the R421Axx relay boards this is the address or channel number.
  In a modbus transaction this is the address field.

For efficiency, nodes aren't implemented as objects, just identifiers used by a Device / Controller.
Nodes can be controlled using generic commands such as 'open', 'close', 'toggle', etc.

.. doxygennamespace:: IO::Modbus
   :members:
