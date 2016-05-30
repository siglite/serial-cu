/* tportc.c
   Handle a Taylor UUCP port command.

   Copyright (C) 1992, 1993, 2002 Ian Lance Taylor

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
const char _uuconf_tportc_rcsid[] = "$Id$";
#endif

#include <errno.h>

static int ipproto_param P((pointer pglobal, int argc, char **argv,
			    pointer pvar, pointer pinfo));
static int ipbaud_range P((pointer pglobal, int argc, char **argv,
			   pointer pvar, pointer pinfo));
static int ipcunknown P((pointer pglobal, int argc, char **argv,
			 pointer pvar, pointer pinfo));

/* The string names of the port types.  This array corresponds to the
   uuconf_porttype enumeration.  */

static const char * const azPtype_names[] =
{
  NULL,
  "stdin",
  "direct",
  "pipe"
};

#define CPORT_TYPES (sizeof azPtype_names / sizeof azPtype_names[0])

/* The command table for generic port commands.  The "port" and "type"
   commands are handled specially.  */
static const struct cmdtab_offset asPort_cmds[] =
{
  { "protocol", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_zprotocols), NULL },
  { "protocol-parameter", UUCONF_CMDTABTYPE_FN | 0,
      offsetof (struct uuconf_port, uuconf_qproto_params), ipproto_param },
  { "seven-bit", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable), _uuconf_iseven_bit },
  { "reliable", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable), _uuconf_ireliable },
  { "half-duplex", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_port, uuconf_ireliable),
      _uuconf_ihalf_duplex },
  { "lockname", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_zlockname), NULL },
  { NULL, 0, 0, NULL }
};

#define CPORT_CMDS (sizeof asPort_cmds / sizeof asPort_cmds[0])

/* The stdin port command table.  */
static const struct cmdtab_offset asPstdin_cmds[] =
{
  { NULL, 0, 0, NULL }
};

#define CSTDIN_CMDS (sizeof asPstdin_cmds / sizeof asPstdin_cmds[0])

/* The direct port command table.  */
static const struct cmdtab_offset asPdirect_cmds[] =
{
  { "device", UUCONF_CMDTABTYPE_STRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_zdevice),
      NULL },
  { "baud", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_ibaud),
      NULL },
  { "speed", UUCONF_CMDTABTYPE_LONG,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_ibaud),
      NULL },
  { "carrier", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_fcarrier),
      NULL },
  { "hardflow", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_port, uuconf_u.uuconf_sdirect.uuconf_fhardflow),
      NULL },
  { NULL, 0, 0, NULL }
};

#define CDIRECT_CMDS (sizeof asPdirect_cmds / sizeof asPdirect_cmds[0])

/* The pipe port command table.  */
static const struct cmdtab_offset asPpipe_cmds[] =
{
  { "command", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_port, uuconf_u.uuconf_spipe.uuconf_pzcmd),
      NULL },
  { NULL, 0, 0, NULL}
};

#define CPIPE_CMDS (sizeof asPpipe_cmds / sizeof asPpipe_cmds[0])

#undef max
#define max(i1, i2) ((i1) > (i2) ? (i1) : (i2))
#define CCMDS \
  max (max (CPORT_CMDS, CSTDIN_CMDS), max (CDIRECT_CMDS, CPIPE_CMDS))

/* Handle a command passed to a port from a Taylor UUCP configuration
   file.  This can be called when reading either the port file or the
   sys file.  The return value may have UUCONF_CMDTABRET_KEEP set, but
   not UUCONF_CMDTABRET_EXIT.  It assigns values to the elements of
   qport.  The first time this is called, qport->uuconf_zname and
   qport->uuconf_palloc should be set and qport->uuconf_ttype should
   be UUCONF_PORTTYPE_UNKNOWN.  */

int
_uuconf_iport_cmd (struct sglobal *qglobal, int argc, char **argv, struct uuconf_port *qport)
{
  boolean fgottype;
  const struct cmdtab_offset *qcmds;
  size_t ccmds;
  struct uuconf_cmdtab as[CCMDS];
  size_t i;
  int iret;

  fgottype = strcasecmp (argv[0], "type") == 0;

  if (fgottype || qport->uuconf_ttype == UUCONF_PORTTYPE_UNKNOWN)
    {
      enum uuconf_porttype ttype;

      /* We either just got a "type" command, or this is an
	 uninitialized port.  If the first command to a port is not
	 "type", it is assumed to be a modem port.  This
	 implementation will actually permit "type" at any point, but
	 will effectively discard any type specific information that
	 appears before the "type" command.  This supports defaults,
	 in that the default may be of a specific type while future
	 ports in the same file may be of other types.  */
      if (fgottype)
	{
	  if (argc != 2)
	    return UUCONF_SYNTAX_ERROR;

	  for (i = 0; i < CPORT_TYPES; i++)
	    if (azPtype_names[i] != NULL
		&& strcasecmp (argv[1], azPtype_names[i]) == 0)
	      break;

	  if (i >= CPORT_TYPES)
	    return UUCONF_SYNTAX_ERROR;
	  
	  ttype = (enum uuconf_porttype) i;
	}

      qport->uuconf_ttype = ttype;

      switch (ttype)
	{
	default:
	case UUCONF_PORTTYPE_STDIN:
	  break;
	case UUCONF_PORTTYPE_DIRECT:
	  qport->uuconf_u.uuconf_sdirect.uuconf_zdevice = NULL;
	  qport->uuconf_u.uuconf_sdirect.uuconf_ibaud = -1;
	  qport->uuconf_u.uuconf_sdirect.uuconf_fcarrier = FALSE;
	  qport->uuconf_u.uuconf_sdirect.uuconf_fhardflow = TRUE;
	  break;
	case UUCONF_PORTTYPE_PIPE:
	  qport->uuconf_u.uuconf_spipe.uuconf_pzcmd = NULL;
	  break;
	}

      if (fgottype)
	return UUCONF_CMDTABRET_CONTINUE;
    }

  /* See if this command is one of the generic ones.  */
  qcmds = asPort_cmds;
  ccmds = CPORT_CMDS;

  for (i = 0; i < CPORT_CMDS - 1; i++)
    if (strcasecmp (argv[0], asPort_cmds[i].zcmd) == 0)
      break;

  if (i >= CPORT_CMDS - 1)
    {
      /* It's not a generic command, so we must check the type
	 specific commands.  */
      switch (qport->uuconf_ttype)
	{
	case UUCONF_PORTTYPE_STDIN:
	  qcmds = asPstdin_cmds;
	  ccmds = CSTDIN_CMDS;
	  break;
	case UUCONF_PORTTYPE_DIRECT:
	  qcmds = asPdirect_cmds;
	  ccmds = CDIRECT_CMDS;
	  break;
	case UUCONF_PORTTYPE_PIPE:
	  qcmds = asPpipe_cmds;
	  ccmds = CPIPE_CMDS;
	  break;
	default:
	  return UUCONF_SYNTAX_ERROR;
	}
    }

  /* Copy the command table onto the stack and modify it to point to
     qport.  */
  _uuconf_ucmdtab_base (qcmds, ccmds, (char *) qport, as);

  iret = uuconf_cmd_args ((pointer) qglobal, argc, argv, as,
			  (pointer) qport, ipcunknown, 0,
			  qport->uuconf_palloc);

  return iret &~ UUCONF_CMDTABRET_EXIT;
}

/* Handle the "protocol-parameter" command.  */

static int
ipproto_param (pointer pglobal, int argc, char **argv, pointer pvar, pointer pinfo)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct uuconf_proto_param **pqparam = (struct uuconf_proto_param **) pvar;
  struct uuconf_port *qport = (struct uuconf_port *) pinfo;

  return _uuconf_iadd_proto_param (qglobal, argc - 1, argv + 1, pqparam,
				   qport->uuconf_palloc);
}

/* Handle the "baud-range" command.  */

/*ARGSUSED*/
static int
ipbaud_range (pointer pglobal, int argc ATTRIBUTE_UNUSED, char **argv, pointer pvar, pointer pinfo ATTRIBUTE_UNUSED)
{
  /* FIXME: */
  return 0;
}

/* Give an error for an unknown port command.  */

/*ARGSUSED*/
static int
ipcunknown (pointer pglobal ATTRIBUTE_UNUSED, int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED, pointer pvar ATTRIBUTE_UNUSED, pointer pinfo ATTRIBUTE_UNUSED)
{
  return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;
}
