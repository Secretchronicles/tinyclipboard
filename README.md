tinyclipboard
=============

tinyclipboard is a cross-platform C library for accessing the
operating system’s clipboard. It is intended to be as standalone as
possible and not depend on any external libraries except those
absolutely required for clipboard access (e.g. libX11 for X
servers). Especially it is independant from any large GUI toolkit
libraries.

tinyclipboard gives access to read and write text strings from and to
the clipboard. Images and other nontextual data formats are not
supported.

The library puts a strong focus on UTF-8 text. Any string you retrieve
from the clipboard will be returned to you in UTF-8, and any string
you pass is expected to be encoded in UTF-8. Under the hood,
tinyclipboard takes care of converting the UTF-8 string into the
encoding used by the underlying clipboard system.

Supported systems
-----------------

These operating systems and graphics stacks are supported currently:

* Linux systems using X11
* Windows

Building
--------

You can build and use the tinyclipboard library in one of three ways:

1. Compile and use it as a static library as part of your project
   (recommended)
2. Compile and use it directly in your project
3. Compile and use it as a dynamic library

The tinyclipboard library consists of one header file and one C
sourcecode file. It is recommended to build tinyclipboard as part of
your project and link it into your executable as a static library
(option 1 above). Since it is such a small library, you might also
consider to simply drop the two files directly into your project
without a separate library step (option 2, see below for more
information). Finally, tinyclipboard’s Makefile also builds a dynamic
library (option 3).

To build the static and dynamic library files, issue

~~~~~~~~~~~~~~~~~~~~
$ make
~~~~~~~~~~~~~~~~~~~~

in the project directory. You will be left with both a `.a` file
(static library) and a `.so` file (dynamic library) in the project
root directory.

If you wish to install the tinyclipboard library to your system
instead of building it as part of your project, you can issue the
usual

~~~~~~~~~~~~~~~~~~~~
$ make install
~~~~~~~~~~~~~~~~~~~~

command. The `install` task understands the usual `PREFIX` and
`DESTDIR` variables in case you need them.

If you want to build the tinyclipboard library as part of your
project, simply drop the header and C source code file into your
source tree and have your preferred build system compile and link them
in. The only thing to consider here is that you need to link in libX11
(`-lX11`) when you build your program for an X11 system. If you are
building an application with a graphical user interface, chances are
high that you need to link in libX11 anyawy.

Examples
--------

There are some examples of use provided in the `examples/` directory
in the source tree. To build them, issue one of:

~~~~~~~~~~~~~~~~~~~~
$ make examples_x11   # X11 systems
$ make examples_win32 # Windows systems
~~~~~~~~~~~~~~~~~~~~

in the toplevel directory. The different commands are to accomodate
the different linking needs (X11 systems need `-lX11` to be linked in).

Usage
-----

The tinyclipboard library provides three main functions:

* `tiny_clipread()`, which returns the content of the clipboard.
* `tiny_clipwrite()`, which replaces the content of the clipboard.
* `tiny_clipnwrite()`, which is the same as `tiny_clipwrite()` with
  the exception that it allows to embed NUL bytes into the clipboard
  on X11.

For version information, the `tiny_clipversion()` function is
available.

Minimal example of how to read from the clipboard:

~~~~~~~~~~~~~~~~~~~~ c
#include <stdlib.h>
#include <stdio.h>
#include <tinyclipboard.h>

int main()
{
  char* str = tiny_clipread(NULL);
  if (str) {
    printf("The clipboard contains: '%s'\n", str);
    free(str);
  }
  else {
    printf("No text in clipboard.\n");
  }

  return 0;
}
~~~~~~~~~~~~~~~~~~~~

Writing is a little more difficult especially on X11 systems. Refer to
the tiny_clipwrite(3) and tiny_clipnwrite(3) manpages.

Documentation
-------------

Full and detailed documentation of the functions provided by the
tinyclipboard library is available in form of manpages in the `man/`
directory of the source tree. Each function is described in detail,
usually with examples.

Contact information
-------------------

The tinyclipboard library was written by Marvin Gülker
<m-guelker@guelkerdev.de>.

If you find any bugs or want to contribute to the tinyclipboard
library, feel free to contact me under this email address.

Licensing
---------

For normal use, I offer the tinyclipboard library under the terms of
the GNU GPLv3+ license. For other licensing options, please contact me
under the email address mentioned above.

### GPL statement

tinyclipboard is a cross-platform clipboard library written in C.
Copyright © 2016 Marvin Gülker

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
