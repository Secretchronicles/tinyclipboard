/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#include <stdio.h>
#include <unistd.h>
#include "tinyclipboard.h"

int main()
{
#ifdef _WIN32
  /* Fix buffer problem with mintty. */
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

  tiny_clipwrite("This is a test.\n");

  /* Clipboard may vanish on X11 if we exit, so don't. */
  for(;;)
    sleep(1);

  return 0;
}
