/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#include <stdlib.h>
#include <stdio.h>
#include "tinyclipboard.h"

int main()
{
  /* Full example with length query */
  int len = 0;
  char* str = tiny_clipread(&len);

  if (str) {
    printf("The clipboard contains %d bytes: '%s'\n", len, str);
    free(str);
  }
  else {
    printf("No text in clipboard or no clipboard owner.\n");
  }

  /* Query for a NUL-terminated string only */
  str = tiny_clipread(NULL);
  if (str) {
    printf("The clipboard contains: '%s'\n", str);
    free(str);
  }
  else {
    printf("No text in clipboard or no clipboard owner.\n");
  }

  return 0;
}
