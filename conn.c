/* conn.c
   Connection routines for the Taylor UUCP package.

   Copyright (C) 1991, 1992, 1993, 1994, 2002 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

#include "uucp.h"

#if USE_RCS_ID
const char conn_rcsid[] = "$Id$";
#endif

#include <ctype.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"

/* Create a new connection.  This relies on system dependent functions
   to set the qcmds and psysdep fields.  If qport is NULL, it opens a
   standard input port, in which case ttype is the type of port to
   use.  */

boolean
fconn_init (struct uuconf_port *qport, struct sconnection *qconn, enum uuconf_porttype ttype)
{
  qconn->qport = qport;
  switch (qport == NULL ? ttype : qport->uuconf_ttype)
    {
    case UUCONF_PORTTYPE_STDIN:
      return fsysdep_stdin_init (qconn);
    case UUCONF_PORTTYPE_DIRECT:
      return fsysdep_direct_init (qconn);
    case UUCONF_PORTTYPE_PIPE:
      return fsysdep_pipe_init (qconn);
    default:
      ulog (LOG_ERROR, "Unknown or unsupported port type");
      return FALSE;
    }
}

/* Connection dispatch routines.  */

/* Free a connection.  */

void
uconn_free (struct sconnection *qconn)
{
  (*qconn->qcmds->pufree) (qconn);
}

/* Lock a connection.   */

boolean
fconn_lock (struct sconnection *qconn, boolean fin, boolean fuser)
{
  boolean (*pflock) P((struct sconnection *, boolean, boolean));

  pflock = qconn->qcmds->pflock;
  if (pflock == NULL)
    return TRUE;
  return (*pflock) (qconn, fin, fuser);
}

/* Unlock a connection.  */

boolean
fconn_unlock (struct sconnection *qconn)
{
  boolean (*pfunlock) P((struct sconnection *));

  pfunlock = qconn->qcmds->pfunlock;
  if (pfunlock == NULL)
    return TRUE;
  return (*pfunlock) (qconn);
}

/* Open a connection.  */

boolean
fconn_open (struct sconnection *qconn, long int ibaud, long int ihighbaud, boolean fwait, boolean fuser)
{
  boolean fret;

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_PORT))
    {
      char abspeed[20];

      if (ibaud == (long) 0)
	strcpy (abspeed, "default speed");
      else
	sprintf (abspeed, "speed %ld", ibaud);

      if (qconn->qport == NULL)
	ulog (LOG_DEBUG, "fconn_open: Opening stdin port (%s)",
	      abspeed);
      else if (qconn->qport->uuconf_zname == NULL)
	ulog (LOG_DEBUG, "fconn_open: Opening unnamed port (%s)",
	      abspeed);
      else
	ulog (LOG_DEBUG, "fconn_open: Opening port %s (%s)",
	      qconn->qport->uuconf_zname, abspeed);
    }
#endif

  /* If the system provides a range of baud rates, we select the
     highest baud rate supported by the port.  */
  if (ihighbaud != 0 && qconn->qport != NULL)
    {
      struct uuconf_port *qport;

      qport = qconn->qport;
      ibaud = ihighbaud;
      if (qport->uuconf_ttype == UUCONF_PORTTYPE_DIRECT)
	{
	  if (qport->uuconf_u.uuconf_sdirect.uuconf_ibaud != 0)
	    ibaud = qport->uuconf_u.uuconf_sdirect.uuconf_ibaud;
	}
    }

  /* This will normally be overridden by the port specific open
     routine.  */
  if (qconn->qport == NULL)
    ulog_device ("stdin");
  else
    ulog_device (qconn->qport->uuconf_zname);

  fret = (*qconn->qcmds->pfopen) (qconn, ibaud, fwait, fuser);

  if (! fret)
    ulog_device ((const char *) NULL);

  return fret;
}

/* Close a connection.  */

boolean
fconn_close (struct sconnection *qconn, pointer puuconf, struct dummy *dummy, boolean fsuccess)
{
  boolean fret;

  DEBUG_MESSAGE0 (DEBUG_PORT, "fconn_close: Closing connection");

  /* Don't report hangup signals while we're closing.  */
  fLog_sighup = FALSE;

  fret = (*qconn->qcmds->pfclose) (qconn, puuconf, dummy, fsuccess);

  /* Ignore any SIGHUP we may have gotten, and make sure any signal
     reporting has been done before we reset fLog_sighup.  */
  afSignal[INDEXSIG_SIGHUP] = FALSE;
  ulog (LOG_ERROR, (const char *) NULL);
  fLog_sighup = TRUE;

  ulog_device ((const char *) NULL);

  return fret;
}


/* Read data from the connection.  */

boolean
fconn_read (struct sconnection *qconn, char *zbuf, size_t *pclen, size_t cmin, int ctimeout, boolean freport)
{
  boolean fret;

  fret = (*qconn->qcmds->pfread) (qconn, zbuf, pclen, cmin, ctimeout,
				  freport);

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_INCOMING))
    udebug_buffer ("fconn_read: Read", zbuf, *pclen);
  else if (FDEBUGGING (DEBUG_PORT))
    ulog (LOG_DEBUG, "fconn_read: Read %lu", (unsigned long) *pclen);
#endif

  return fret;
}

/* Write data to the connection.  */

boolean
fconn_write (struct sconnection *qconn, const char *zbuf, size_t clen)
{
#if DEBUG > 1
  if (FDEBUGGING (DEBUG_OUTGOING))
    udebug_buffer ("fconn_write: Writing", zbuf, clen);
  else if (FDEBUGGING (DEBUG_PORT))
    ulog (LOG_DEBUG, "fconn_write: Writing %lu", (unsigned long) clen);
#endif

  return (*qconn->qcmds->pfwrite) (qconn, zbuf, clen);
}

/* Read and write data.  */

boolean
fconn_io (struct sconnection *qconn, const char *zwrite, size_t *pcwrite, char *zread, size_t *pcread)
{
  boolean fret;
#if DEBUG > 1
  size_t cwrite = *pcwrite;
  size_t cread = *pcread;

  if (cread == 0 || cwrite == 0)
    ulog (LOG_FATAL, "fconn_io: cread %lu; cwrite %lu",
	  (unsigned long) cread, (unsigned long) cwrite);
#endif

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_OUTGOING))
    udebug_buffer ("fconn_io: Writing", zwrite, cwrite);
#endif

  fret = (*qconn->qcmds->pfio) (qconn, zwrite, pcwrite, zread, pcread);

  DEBUG_MESSAGE4 (DEBUG_PORT,
		  "fconn_io: Wrote %lu of %lu, read %lu of %lu",
		  (unsigned long) *pcwrite, (unsigned long) cwrite,
		  (unsigned long) *pcread, (unsigned long) cread);

#if DEBUG > 1
  if (*pcread > 0 && FDEBUGGING (DEBUG_INCOMING))
    udebug_buffer ("fconn_io: Read", zread, *pcread);
#endif

  return fret;
}

/* Send a break character to a connection.  Some port types may not
   support break characters, in which case we just return TRUE.  */

boolean
fconn_break (struct sconnection *qconn)
{
  boolean (*pfbreak) P((struct sconnection *));

  pfbreak = qconn->qcmds->pfbreak;
  if (pfbreak == NULL)
    return TRUE;

  DEBUG_MESSAGE0 (DEBUG_PORT, "fconn_break: Sending break character");

  return (*pfbreak) (qconn);
}

/* Change the setting of a connection.  Some port types may not
   support this, in which case we just return TRUE.  */

boolean
fconn_set (struct sconnection *qconn, enum tparitysetting tparity, enum tstripsetting tstrip, enum txonxoffsetting txonxoff)
{
  boolean (*pfset) P((struct sconnection *, enum tparitysetting,
		      enum tstripsetting, enum txonxoffsetting));

  pfset = qconn->qcmds->pfset;
  if (pfset == NULL)
    return TRUE;

  DEBUG_MESSAGE3 (DEBUG_PORT,
		  "fconn_set: Changing setting to %d, %d, %d",
		  (int) tparity, (int) tstrip, (int) txonxoff);

  return (*pfset) (qconn, tparity, tstrip, txonxoff);
}

/* Require or ignore carrier on a connection.  */

boolean
fconn_carrier (struct sconnection *qconn, boolean fcarrier)
{
  boolean (*pfcarrier) P((struct sconnection *, boolean));

  pfcarrier = qconn->qcmds->pfcarrier;
  if (pfcarrier == NULL)
    return TRUE;
  return (*pfcarrier) (qconn, fcarrier);
}

/* Get the baud rate of a connection.  */

long
iconn_baud (struct sconnection *qconn)
{
  long (*pibaud) P((struct sconnection *));

  pibaud = qconn->qcmds->pibaud;
  if (pibaud == NULL)
    return 0;
  return (*pibaud) (qconn);
}

