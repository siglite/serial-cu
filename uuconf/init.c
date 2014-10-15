/* init.c
   Initialize for reading UUCP configuration files.

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
const char _uuconf_init_rcsid[] = "$Id$";
#endif

/* Initialize the UUCP configuration file reading routines.  This is
   just a generic routine which calls the type specific routines.  */

/*ARGSUSED*/
int
uuconf_init (pointer *ppglobal, const char *zprogram, const char *zname)
{
  struct sglobal **pqglob = (struct sglobal **) ppglobal;
  int iret;

  iret = UUCONF_NOT_FOUND;

  *pqglob = NULL;

#if HAVE_TAYLOR_CONFIG
  iret = uuconf_taylor_init (ppglobal, zprogram, zname);
  if (iret != UUCONF_SUCCESS)
    return iret;
#endif

#if HAVE_V2_CONFIG
  if (*pqglob == NULL || (*pqglob)->qprocess->fv2)
    {
      iret = uuconf_v2_init (ppglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }
#endif

#if HAVE_HDB_CONFIG
  if (*pqglob == NULL || (*pqglob)->qprocess->fhdb)
    {
      iret = uuconf_hdb_init (ppglobal, zprogram);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }
#endif

  return iret;
}
