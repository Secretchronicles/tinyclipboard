/* tinyclipboard - a cross-platform C library for accessing the clipboard.
 *
 * Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
 *
 * All rights reserved. See the README and LICENSE files for the
 * licensing conditions.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tinyclipboard.h"

int main()
{
#ifdef _WIN32
  /* Fix buffer problem with mintty. */
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

  printf("Writing 'This is a test' to the clipboard.\n");
  if (tiny_clipwrite("This is a test.") != -1)
    printf("Success.\n");
  else
    printf("Failure.\n");

  sleep(10);

  printf("Writing 'another test' to the clipboard.\n");
  if (tiny_clipnwrite("another test", strlen("another test")) != -1)
      printf("Success.\n");
  else
    printf("Failure.\n");

  sleep(10);

  printf("Exiting.\n");

  return 0;
}
