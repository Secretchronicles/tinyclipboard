/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#ifndef TINYCLIPBOARD_H
#define TINYCLIPBOARD_H
#define TINYCLIPBOARD_VERSION 20160100L
#define TINYCLIPBOARD_VERSION_POSTFIX ""

const char* tiny_clipversion();
char* tiny_clipread(int* len);
int tiny_clipwrite(const char* text);
int tiny_clipnwrite(const char* text, int len);

#endif
