Termu - ADM3a (for now) X11 terminal emulator
=============================================

About
-----
I needed to test the termtris ADM3a backend, so I decided to write an ADM3a
terminal emulator. Eventually if the stars align I might continue hacking on it
and add support for more emulated terminals, so I decided on a generic project
name. Another potential vector of improvement is to make it look like the real
terminal as much as possible, which is why I decided to use OpenGL for drawing.

For now it's incomplete and extremely bare-bones, but enough to verify that
termtris does in fact work on the ADM3a.

License
-------
Copyright (c) 2023 John Tsiombikas <nuclear@mutantstargoat.com>

This program is free software. Feel free to use, modify, and/or redistribute it,
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation. See COPYING for
details.

Build
-----
The repo does not include the binary character generator ROMs of the ADM3a. Grab
the `data` directory from a release archive.

Run `make` to build.
