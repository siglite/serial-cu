/* portnm.c
   Get the port name of stdin.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

/* Get the port name of standard input.  I assume that Unix systems
   generally support ttyname.  If they don't, this function can just
   return NULL.  */

const char *
zsysdep_port_name (boolean *ftcp_port)
{
  const char *z;

  z = ttyname (0);
  if (z == NULL)
    return NULL;
  if (strncmp (z, "/dev/", sizeof "/dev/" - 1) == 0)
    return z + sizeof "/dev/" - 1;
  else
    return z;
}
