// Author Woohyuk Jang, Date Nov 7 2016
// error checking sanity function

#ifndef _CHECK_H
#define _CHECK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// for error checking sanity
static void errcheck(char *string, int rc) {
  // if return code is not 0, then you have an error peasant
  if (rc) {
    fprintf(stderr, "Error: %s, ReturnCode:%d", string, rc);
    exit(EXIT_FAILURE);
  }
  return;
}

#endif
