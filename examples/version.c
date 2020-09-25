/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#include <stdio.h>
#include "tinyclipboard.h"

int main()
{
  printf("%s\n", tiny_clipversion());
  return 0;
}
