/* sinfo.c
   Get information about a system.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_sinfo_rcsid[] = "$Id$";
#endif

/* Get information about a particular system.  We combine the
   definitions for this system from each type of configuration file,
   by passing what we have so far into each one.  */

int
uuconf_system_info (pointer pglobal, const char *zsystem, struct uuconf_system *qsys)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  boolean fgot;

  fgot = FALSE;

  iret = _uuconf_itaylor_system_internal (qglobal, zsystem, qsys);
  if (iret == UUCONF_SUCCESS)
    fgot = TRUE;
  else if (iret != UUCONF_NOT_FOUND)
    return iret;

  if (! fgot)
    return UUCONF_NOT_FOUND;

  return _uuconf_isystem_basic_default (qglobal, qsys);
}
