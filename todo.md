# IOControl Subsystem

## Filesystem integration

Could have a use to support asynchronous file transfers. This would complement Sming's network capabilities by supporting slow devices which require background data/control transfers. Both I/O and data can be handled by the same system: read() / write() / ioctl()

## Configuration API

Abstracting this so it's not dependent on JSON. Can support features like automatically closing config file after a timeout.

## Status / Error codes
 
We currently use both request\_status\_t and ioerror\_t but as status only has 3 values it would simplify things to drop it; we'd define negative values for errors, 0 for success and 1 for pending. We can still call it ioerror_t.

## Control Enable / Disable

Perhaps add socket command to enable/disable control sequences.
