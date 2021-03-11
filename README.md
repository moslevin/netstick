# netstick

## What is it?

Netstick enables HID devices to be remotely connected between a "client" and "server" over a network connection.

It allows the keyboards, mice, joysticks, and other HID devices physically attached to one computer to seamlessly connect to another.
All input events detected on the client are broadcast in real-time to the server and interpreted as if the devices were attached to it.

The client and server are lightweight and efficient -- written in high-performance C with zero external library dependencies (on Linux).

While netstick currently implements a linux host and server, the wire protocol is designed such that any networked devices can implement
the client and server roles without assuming a particular network technology or underlying host OS.

## Why write it?

My reason for writing netstick is to provide a convenient way to use a bunch of wired USB gamepads with a raspberry pi connected to a TV; where
the layout of the room make it impractical to connect them directly, and where USB extension cords would be an eyesore and a tripping hazard.
cords.  

The idea was to have a conveniently-located secondary Raspberry Pi act as a wifi-enabled joystick hub running multiple instances of the netstick 
client, transmitting data from the connected joysticks over wifi to the primary Raspberry Pi.

In the design process I realized that the mechanism that I used to implement the joystick hub would "just work" for other HID devices connected
to the hub, and that functionality would come for free.

## What works?

netstickd (server):
- Single-threaded event-driven server using epoll
- Multiple concurrent connections from multiple clients
- Register remote Keyboard, Mouse, and Joystick devices locally using Linux uinput module
- Analog (Absolute axis, Relative axis) events
- Digital (keyboard/mouse/joystick button) events

netstick (client):
- Single-threaded, single-device client
- Enumerate local HID devices and transmit configuration to remote device creation
- Analog (Absolute axis, Relative axis) events
- Digital (keyboard/mouse/joystick button) events

protocol:
- Tag/length/value/checksum message format 
- slip-encoding of message frames
- device-registration message format
- TCP/IP (IPv4) connections

## What doesn't work?

- Any server-to-client features - such as force-feedback, programmable LEDs, etc.
- Keyboard repeat-rate messages
- Handle multiple HID devices with a single netstick client interface (although multiple netstick clients can be run on a device)

## ToDo's

- Optimize data structures sent over-the-wire.
- Add a keyboard-emulation mode for joystick events.
- Add IPv6 support

## Building
`
	$ cmake ./CMakeLists.txt
	$ make
`

## Running

netstickd (server):

`	
	$ ./netstickd <port>
`

	Where:
	- port is the network port that the server will listen on for incoming connections

	NOTE: 
	- as netstickd registers devices with uinput, you need to ensure that the user is a member of a group capable registering uinput devices (or run it as root)

netstick (client):
`
	$ ./netstick <source> <ip> <port>
`	

	Where:
	- source is the path the uinput device to forward over the network (i.e. /dev/input/eventX)
	- ip address of the server
	- port on the server to connect to 

## License

Copyright (c) 2021, Funkenstein Software Consulting

This program is BSD licensed software, See LICENSE.txt for more details.

