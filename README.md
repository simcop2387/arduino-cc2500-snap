# Description

This library implements a subset of [SNAP](http://www.hth.com/snap/ "SNAP Documentation") on top of CC2500 style radios.

# Supported Features

* 0-24bit addresses for destinations
* 8-24bit addresses for sources
* Up to 53 bytes per packet

# Unsupported

## Likely to add
* CRC16/32
> Likely CRC16 since it'll be more in line with the AVRs abilities

* ACK/NAK
> Needs some extra planning for it to work well

* Command bit protocol
> Likely to support using [nanopb](http://koti.kapsi.fi/jpa/nanopb/) to describe how to talk to the device

* Longer packet support
> Needs interrupt driven reception, not built on my boards right now.  Would also mean that devices need to support throwing out packets longer than they support

## Thinking about
* Automatic address assignment
> Needs extra support such as the command bit to allow for device discovery

* Protocol buffers
> Build the Level 6-7 part of the network with protocol buffers, using [nanopb](http://koti.kapsi.fi/jpa/nanopb/) most likely, to allow for easy implementation everywhere

## Not going to add
* FEC
> FEC would require far more ram that I'm comfortable with on an AVR, adding it wouldn't benefit things much more than CRC16

