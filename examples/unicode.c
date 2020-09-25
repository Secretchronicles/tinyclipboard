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

  /* If your editor does not display all these Unicode characters,
   * you need to switch to a font that contains them. Similaryly,
   * if the application you paste this text into does not display
   * the characters, you need to set it to use a different font that
   * contains the glyphs in question. */
  tiny_clipwrite("Umlaut ü, Buckel-ß, greek β. This is cool™. More‽ ☺\n");

  /* Clipboard may vanish on X11 if we exit, so don't. */
  for(;;)
    sleep(1);

  return 0;
}
