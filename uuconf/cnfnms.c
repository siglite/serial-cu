/* cnfnms.c
   Return configuration file names.

   Copyright (C) 2002 Ian Lance Taylor

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
const char _uuconf_cnfnms_rcsid[] = "$Id$";
#endif

#include <errno.h>

int
uuconf_config_files (pointer pglobal, struct uuconf_config_file_names *qnames)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;

  qnames->uuconf_ztaylor_config = qglobal->qprocess->zconfigfile;
  qnames->uuconf_pztaylor_sys =
    (const char * const *) qglobal->qprocess->pzsysfiles;
  qnames->uuconf_pztaylor_port =
    (const char * const *) qglobal->qprocess->pzportfiles;
  qnames->uuconf_pztaylor_pwd =
    (const char * const *) qglobal->qprocess->pzpwdfiles;
  qnames->uuconf_pztaylor_call =
    (const char * const *) qglobal->qprocess->pzcallfiles;

  return UUCONF_SUCCESS;
}
