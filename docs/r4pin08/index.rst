R4PIN08 8-channel I/O board
===========================

.. image:: r4pin08.jpg

I/O lines connected to 5V microcontroller via series 470R, output drive 10mA max.
Input pull up/down approx. 50K.

- Supports MODBUS coil read/write
- Supports holding register read/write:
   - 245 Input polarity, default 0 (active LOW), set to 1 for active HIGH
      - 0: Inputs idle at 5v, read as active when pulled low
      - 1: Inputs idle at 0v, read as active when pulled high
   - 246 Output polarity, default 0 
      - 0: Output idles at 5v, set to 0v when asserted
      - 1: Output idles at 0v, set to 5v when asserted
   - 247 unknown, default 2
   - 253 slave ID, default 1
   - 254 unknown, default 3

MODBUS commands supported:
   - ReadCoils: read state of outputs
   - WriteSingleCoil
   - WriteMultipleCoils
   - ReadDiscreteInputs: read state of inputs
   - ReadHoldingRegisters
   - WriteSingleRegister
   - WriteMultipleRegisters


.. doxygennamespace:: IO::Modbus::R4PIN08
   :members:
