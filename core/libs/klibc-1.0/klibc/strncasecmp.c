/*
 * strncasecmp.c
 */

#include <string.h>
#include <ctype.h>

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  const unsigned char *c1 = s1, *c2 = s2;
  unsigned char ch;
  int d = 0;

  while ( n-- ) {
    /* toupper() expects an unsigned char (implicitly cast to int)
       as input, and returns an int, which is exactly what we want. */
    d = toupper(ch = *c1++) - toupper(*c2++);
    if ( d || !ch )
      break;
  }

  return d;
}
