/* uustat.c
   UUCP status program

   Copyright (C) 1991, 1992 Ian Lance Taylor

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#if USE_RCS_ID
const char uustat_rcsid[] = "$Id$";
#endif

#include <ctype.h>
#include <errno.h>

#if HAVE_TIME_H
#include <time.h>
#endif

#include "system.h"
#include "getopt.h"

/* The uustat program permits various listings and manipulations of
   files in the spool directory.  This implementation supports the
   following switches:

   -a list all jobs
   -Blines number of lines of standard input to mail
   -ccommand list only executions of specified command
   -Ccommand list only jobs other than executions of specified command
   -e list execute jobs rather than command requests
   -i ask user whether to kill each listed job
   -Ifile set configuration file name
   -kjobid kill job with specified ID
   -K kill each listed job
   -m report status for all remote machines
   -M mail uucp about each job killed with -K
   -N mail requestor about each job killed with -K
   -ohour report jobs older than specified number of hours
   -p do "ps -flp" on all processes holding lock files (Unix specific)
   -q list number of jobs for all systems
   -Q don't list jobs, just do -K processing
   -rjobid rejuvenate job with specified ID
   -ssystem report on all jobs for specified system
   -Ssystem report on all jobs other than for specified system
   -uuser report on all jobs for specified user
   -Uuser report on all jobs other than for specified user
   -Wcomment comment to include in mail messages
   -xdebug set debugging level
   -yhour report jobs younger than specified number of hours  */

/* The program name.  */
char abProgram[] = "uustat";

/* What to do with a job that matches the selection criteria; these
   values may be or'red together.  */
#define JOB_SHOW (01)
#define JOB_INQUIRE (02)
#define JOB_KILL (04)
#define JOB_MAIL (010)
#define JOB_NOTIFY (020)

/* This structure is used to accumulate all the lines in a single
   command file, so that they can all be displayed at once and so that
   executions can be displayed reasonably.  */

struct scmdlist
{
  struct scmdlist *qnext;
  struct scmd s;
  long itime;
};

/* Local functions.  */

static void ususage P((void));
static boolean fsxqt_file_read P((pointer puuconf, const char *zfile));
static void usxqt_file_free P((void));
static int isxqt_cmd P((pointer puuconf, int argc, char **argv, pointer pvar,
			pointer pinfo));
static int isxqt_file P((pointer puuconf, int argc, char **argv, pointer pvar,
			 pointer pinfo));
static int isxqt_user P((pointer puuconf, int argc, char **argv, pointer pvar,
			 pointer pinfo));
static boolean fsworkfiles P((pointer puuconf, int icmd, int csystems,
			      char **pazsystems, boolean fnotsystems,
			      int cusers, char **pazusers,
			      boolean fnotusers, long iold, long iyoung,
			      int ccommands, char **pazcommands,
			      boolean fnotcommands, const char *zcomment,
			      int cstdin));
static boolean fsworkfiles_system P((pointer puuconf,int icmd,
				     const struct uuconf_system *qsys,
				     int cusers,  char **pazusers,
				     boolean fnotusers, long iold,
				     long iyoung, int ccommands,
				     char **pazcommands,
				     boolean fnotcommands,
				     const char *zcomment, int cstdin));
static boolean fsworkfile_show P((pointer puuconf, int icmd,
				  const struct uuconf_system *qsys,
				  const struct scmd *qcmd,
				  long itime, int ccommands,
				  char **pazcommands, boolean fnotcommands,
				  const char *zcomment, int cstdin));
static void usworkfile_header P((const struct uuconf_system *qsys,
				 const struct scmd *qcmd,
				 const char *zjobid,
				 long itime, boolean ffirst));
static boolean fsexecutions P((pointer puuconf, int icmd, int csystems,
			       char **pazsystems, boolean fnotsystems,
			       int cusers, char **pazusers,
			       boolean fnotusers, long iold, long iyoung,
			       int ccommands, char **pazcommands,
			       boolean fnotcommands, const char *zcomment,
			       int cstdin));
static boolean fsnotify P((pointer puuconf, int icmd, const char *zcomment,
			   int cstdin, boolean fkilled, const char *zcmd,
			   struct scmdlist *qcmd, const char *zid,
			   const char *zuser,
			   const struct uuconf_system *qsys,
			   const char *zstdin, const char *zrequestor));
static boolean fsquery P((pointer puuconf));
static void usunits_show P((long idiff));
static boolean fsmachines P((void));

/* Long getopt options.  */

static const struct option asLongopts[] = { { NULL, 0, NULL, 0 } };

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -a: list all jobs.  */
  boolean fall = FALSE;
  /* -B lines: number of lines of standard input to mail.  */
  int cstdin = 100;
  /* -c,-C command: list only specified command.  */
  int ccommands = 0;
  char **pazcommands = NULL;
  boolean fnotcommands = FALSE;
  /* -e: list execute jobs.  */
  boolean fexecute = FALSE;
  /* -k jobid: kill specified job.  */
  int ckills = 0;
  char **pazkills = NULL;
  /* -m: report machine status.  */
  boolean fmachine = FALSE;
  /* -o hour: report jobs older than given number of hours.  */
  int ioldhours = -1;
  /* -p: report status of jobs holding lock files.  */
  boolean fps = FALSE;
  /* -q: list number of jobs for each system.  */
  boolean fquery = FALSE;
  /* -r jobid: rejuvenate specified job.  */
  int crejuvs = 0;
  char **pazrejuvs = NULL;
  /* -s,-S system: list all jobs for specified system.  */
  int csystems = 0;
  char **pazsystems = NULL;
  boolean fnotsystems = FALSE;
  /* -u,-U user: list all jobs for specified user.  */
  int cusers = 0;
  char **pazusers = NULL;
  boolean fnotusers = FALSE;
  /* -W comment: comment to include in mail messages.  */
  const char *zcomment = NULL;
  /* -y hour: report jobs younger than given number of hours.  */
  int iyounghours = -1;
  /* -I file: set configuration file.  */
  const char *zconfig = NULL;
  /* -Q, -i, -K, -M, -N: what to do with each job.  */
  int icmd = JOB_SHOW;
  int ccmds;
  int iopt;
  pointer puuconf;
  int iuuconf;
  long iold;
  long iyoung;
  const char *azoneuser[1];
  boolean fret;

  while ((iopt = getopt (argc, argv,
			 "aB:c:C:eiI:k:KmMNo:pqQr:s:S:u:U:W:x:y:")) != EOF)
    {
      switch (iopt)
	{
	case 'a':
	  /* List all jobs.  */
	  fall = TRUE;
	  break;

	case 'B':
	  /* Number of lines of standard input to mail.  */
	  cstdin = (int) strtol (optarg, (char **) NULL, 10);
	  break;

	case 'C':
	  /* List jobs for other than specified command.  */
	  fnotcommands = TRUE;
	  /* Fall through.  */
	case 'c':
	  /* List specified command.  */
	  ++ccommands;
	  pazcommands = (char **) xrealloc ((pointer) pazcommands,
					    ccommands * sizeof (char *));
	  pazcommands[ccommands - 1] = optarg;
	  break;

	case 'e':
	  /* List execute jobs.  */
	  fexecute = TRUE;
	  break;

	case 'i':
	  /* Prompt the user whether to kill each job.  */
	  icmd |= JOB_INQUIRE;
	  break;

	case 'I':
	  /* Set configuration file name.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'k':
	  /* Kill specified job.  */
	  ++ckills;
	  pazkills = (char **) xrealloc ((pointer) pazkills,
					 ckills * sizeof (char *));
	  pazkills[ckills - 1] = optarg;
	  break;

	case 'K':
	  /* Kill each listed job.  */
	  icmd |= JOB_KILL;
	  break;

	case 'm':
	  /* Report machine status.  */
	  fmachine = TRUE;
	  break;

	case 'M':
	  /* Mail to uucp action taken on each job.  */
	  icmd |= JOB_MAIL;
	  break;

	case 'N':
	  /*  Mail to requestor action taken on each job.  */
	  icmd |= JOB_NOTIFY;
	  break;

	case 'o':
	  /* Report old jobs.  */
	  ioldhours = (int) strtol (optarg, (char **) NULL, 10);
	  break;

	case 'p':
	  /* Get status of processes holding locks.  */
	  fps = TRUE;
	  break;

	case 'q':
	  /* List number of jobs for each system.  */
	  fquery = TRUE;
	  break;

	case 'Q':
	  /* Don't list jobs, just do -K processing.  */
	  icmd &=~ JOB_SHOW;
	  break;

	case 'r':
	  /* Rejuvenate specified job.  */
	  ++crejuvs;
	  pazrejuvs = (char **) xrealloc ((pointer) pazrejuvs,
					  crejuvs * sizeof (char *));
	  pazrejuvs[crejuvs - 1] = optarg;
	  break;

	case 'S':
	  /* List jobs for other than specified system.  */
	  fnotsystems = TRUE;
	  /* Fall through.  */
	case 's':
	  /* List jobs for specified system.  */
	  ++csystems;
	  pazsystems = (char **) xrealloc ((pointer) pazsystems,
					   csystems * sizeof (char *));
	  pazsystems[csystems - 1] = optarg;
	  break;

	case 'U':
	  /* List jobs for other than specified user.  */
	  fnotusers = TRUE;
	  /* Fall through.  */
	case 'u':
	  /* List jobs for specified user.  */
	  ++cusers;
	  pazusers = (char **) xrealloc ((pointer) pazusers,
					 cusers * sizeof (char *));
	  pazusers[cusers - 1] = optarg;
	  break;

	case 'W':
	  /* Comment to include in mail messages.  */
	  zcomment = optarg;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 'y':
	  /* List jobs younger than given number of hours.  */
	  iyounghours = (int) (optarg, (char **) NULL, 10);
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ususage ();
	  break;
	}
    }

  if (optind != argc)
    ususage ();

  /* To avoid confusion, most options are only permitted by
     themselves.  This restriction might be removed later, but it is
     imposed by most implementations.  We do permit any combination of
     -c, -s, -u, -o and -y, and any combination of -k and -r.  */
  ccmds = 0;
  if (fall)
    ++ccmds;
  if (ckills > 0 || crejuvs > 0)
    ++ccmds;
  if (fmachine)
    ++ccmds;
  if (fps)
    ++ccmds;
  if (fquery)
    ++ccmds;
  if (fexecute || csystems > 0 || cusers > 0 || ioldhours != -1
      || iyounghours != -1 || ccommands > 0)
    ++ccmds;

  if (ccmds > 1)
    {
      ulog (LOG_ERROR, "Too many options");
      ususage ();
    }

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

#if DEBUG > 1
  {
    const char *zdebug;

    iuuconf = uuconf_debuglevel (puuconf, &zdebug);
    if (iuuconf != UUCONF_SUCCESS)
      ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
    if (zdebug != NULL)
      iDebug |= idebug_parse (zdebug);
  }
#endif

  usysdep_initialize (puuconf, 0);

  /* If no commands were specified, we list all commands for the given
     user.  */
  if (ccmds == 0)
    {
      cusers = 1;
      azoneuser[0] = zsysdep_login_name ();
      pazusers = (char **) azoneuser;
    }

  /* Canonicalize the system names.  */
  if (csystems > 0)
    {
      int i;

      for (i = 0; i < csystems; i++)
	{
	  struct uuconf_system ssys;

	  iuuconf = uuconf_system_info (puuconf, pazsystems[i], &ssys);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      if (iuuconf == UUCONF_NOT_FOUND)
		ulog (LOG_FATAL, "%s: System not found", pazsystems[i]);
	      else
		ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
	    }
	  if (strcmp (pazsystems[i], ssys.uuconf_zname) != 0)
	    pazsystems[i] = zbufcpy (ssys.uuconf_zname);
	  (void) uuconf_system_free (puuconf, &ssys);
	}
    }

  if (ioldhours == -1)
    iold = (long) -1;
  else
    {
      iold = (isysdep_time ((long *) NULL)
	      - (long) ioldhours * (long) 60 * (long) 60);
      if (iold < 0L)
	iold = 0L;
    }
  if (iyounghours == -1)
    iyoung = (long) -1;
  else
    {
      iyoung = (isysdep_time ((long *) NULL)
		- (long) iyounghours * (long) 60 * (long) 60);
      if (iyoung < 0L)
	iyoung = 0L;
    }

  if (! fexecute
      && (fall
	  || csystems > 0
	  || cusers > 0
	  || ioldhours != -1
	  || iyounghours != -1
	  || ccommands > 0))
    fret = fsworkfiles (puuconf, icmd, csystems, pazsystems, fnotsystems,
			cusers, pazusers, fnotusers, iold,  iyoung,
			ccommands, pazcommands, fnotcommands, zcomment,
			cstdin);
  else if (fexecute)
    fret = fsexecutions (puuconf, icmd, csystems, pazsystems, fnotsystems,
			 cusers, pazusers, fnotusers, iold, iyoung,
			 ccommands, pazcommands, fnotcommands, zcomment,
			 cstdin);
  else if (icmd != JOB_SHOW)
    {
      ulog (LOG_ERROR,
	    "-i, -K, -M, -N, -Q not supported with -k, -m, -p, -q, -r");
      ususage ();
      fret = FALSE;
    }
  else if (fquery)
    fret = fsquery (puuconf);
  else if (fmachine)
    fret = fsmachines ();
  else if (ckills > 0 || crejuvs > 0)
    {
      int i;

      fret = TRUE;
      for (i = 0; i < ckills; i++)
	if (! fsysdep_kill_job (puuconf, pazkills[i]))
	  fret = FALSE;

      for (i = 0; i < crejuvs; i++)
	if (! fsysdep_rejuvenate_job (puuconf, pazrejuvs[i]))
	  fret = FALSE;
    }
  else if (fps)
    fret = fsysdep_lock_status ();
  else
    {
#if DEBUG > 0
      ulog (LOG_FATAL, "Can't happen");
#endif
      fret = FALSE;
    }

  ulog_close ();

  usysdep_exit (fret);

  /* Avoid errors about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void
ususage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uustat [options]\n");
  fprintf (stderr,
	   " -a: list all UUCP jobs\n");
  fprintf (stderr, 
	   " -B num: number of lines to return in -M or -N mail message\n");
  fprintf (stderr,
	   " -c command: list requests for named command\n");
  fprintf (stderr,
	   " -C command: list requests for other than named command\n");
  fprintf (stderr,
	   " -e: list queued executions rather than job requests\n");
  fprintf (stderr,
	   " -i: prompt for whether to kill each listed job\n");
  fprintf (stderr,
	   " -k job: kill specified UUCP job\n");
  fprintf (stderr,
	   " -K: kill each listed job\n");
  fprintf (stderr,
	   " -m: report status for all remote machines\n");
  fprintf (stderr,
	   " -M: mail report on each listed job to UUCP administrator\n");
  fprintf (stderr,
	   " -N: mail report on each listed job to requestor\n");
  fprintf (stderr,
	   " -o hours: list all jobs older than given number of hours\n");
  fprintf (stderr,
	   " -p: show status of all processes holding UUCP locks\n");
  fprintf (stderr,
	   " -q: list number of jobs for each system\n");
  fprintf (stderr,
	   " -Q: don't list jobs, just take actions (-i, -K, -M, -N)\n");
  fprintf (stderr,
	   " -r job: rejuvenate specified UUCP job\n");
  fprintf (stderr,
	   " -s system: list all jobs for specified system\n");
  fprintf (stderr,
	   " -S system: list all jobs for other than specified system\n");
  fprintf (stderr,
	   " -u user: list all jobs for specified user\n");
  fprintf (stderr,
	   " -U user: list all jobs for other than specified user\n");
  fprintf (stderr,
	   " -W comment: comment to include in mail messages\n");
  fprintf (stderr,
	   " -y hours: list all jobs younger than given number of hours\n");
  fprintf (stderr,
	   " -x debug: Set debugging level (0 for none, 9 is max)\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* We need to be able to read information from an execution file.  */

/* The user name extracted from an execution file.  */
static char *zSxqt_user;

/* The system name from an execution file.  */
static char *zSxqt_system;

/* Address of requesting user (who to send mail to).  */
static const char *zSxqt_requestor;

/* The command (no arguments) from an execution file.  */
static char *zSxqt_prog;

/* The full command line from an execution file.  */
static char *zSxqt_cmd;

/* Number of files associated with an execution file.  */
static int cSxqt_files;

/* Names of files associated with execution file.  */
static char **pazSxqt_files;

/* Standard input file name.  */
static const char *zSxqt_stdin;

/* A command table used to dispatch an execution file.  */
static const struct uuconf_cmdtab asSxqt_cmds[] =
{
  { "C", UUCONF_CMDTABTYPE_FN | 0, NULL, isxqt_cmd },
  { "I", UUCONF_CMDTABTYPE_STRING, &zSxqt_stdin, NULL },
  { "F", UUCONF_CMDTABTYPE_FN | 0, NULL, isxqt_file },
  { "R", UUCONF_CMDTABTYPE_STRING, (pointer) &zSxqt_requestor, NULL },
  { "U", UUCONF_CMDTABTYPE_FN | 3, NULL, isxqt_user },
  { NULL, 0, NULL, NULL }
};

/* Read an execution file, setting the above variables.  */

static boolean
fsxqt_file_read (puuconf, zfile)
     pointer puuconf;
     const char *zfile;
{
  FILE *e;
  int iuuconf;
  boolean fret;

  e = fopen (zfile, "r");
  if (e == NULL)
    {
      ulog (LOG_ERROR, "fopen (%s): %s", zfile, strerror (errno));
      return FALSE;
    }

  zSxqt_user = NULL;
  zSxqt_system = NULL;
  zSxqt_requestor = NULL;
  zSxqt_prog = NULL;
  zSxqt_cmd = NULL;
  cSxqt_files = 0;
  pazSxqt_files = NULL;

  iuuconf = uuconf_cmd_file (puuconf, e, asSxqt_cmds, (pointer) NULL,
			     (uuconf_cmdtabfn) NULL,
			     UUCONF_CMDTABFLAG_CASE, (pointer) NULL);
  (void) fclose (e);
  if (iuuconf == UUCONF_SUCCESS)
    fret = TRUE;
  else
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      fret = FALSE;
    }

  if (zSxqt_user == NULL)
    zSxqt_user = zbufcpy ("*unknown*");
  if (zSxqt_system == NULL)
    zSxqt_system = zbufcpy ("*unknown*");
  if (zSxqt_prog == NULL)
    {
      zSxqt_prog = zbufcpy ("*none*");
      zSxqt_cmd = zbufcpy ("*none*");
    }

  return fret;
}

/* Free up the information read from an execution file.  */

static void
usxqt_file_free ()
{
  int i;

  ubuffree (zSxqt_user);
  ubuffree (zSxqt_system);
  ubuffree (zSxqt_prog);
  ubuffree (zSxqt_cmd);
  for (i = 0; i < cSxqt_files; i++)
    ubuffree (pazSxqt_files[i]);
  xfree ((pointer) pazSxqt_files);
}

/* Get the command from an execution file.  */

/*ARGSUSED*/
static int
isxqt_cmd (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  size_t clen;
  int i;

  if (argc <= 1)
    return UUCONF_CMDTABRET_CONTINUE;

  zSxqt_prog = zbufcpy (argv[1]);

  clen = 0;
  for (i = 1; i < argc; i++)
    clen += strlen (argv[i]) + 1;

  zSxqt_cmd = zbufalc (clen);
  zSxqt_cmd[0] = '\0';
  for (i = 1; i < argc - 1; i++)
    {
      strcat (zSxqt_cmd, argv[i]);
      strcat (zSxqt_cmd, " ");
    }
  strcat (zSxqt_cmd, argv[i]);

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Get the associated files from an execution file.  */

/*ARGSUSED*/
static int
isxqt_file (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  if (argc != 2 && argc != 3)
    return UUCONF_CMDTABRET_CONTINUE;

  /* If this file is not in the spool directory, just ignore it.  */
  if (! fspool_file (argv[1]))
    return UUCONF_CMDTABRET_CONTINUE;

  ++cSxqt_files;
  pazSxqt_files = (char **) xrealloc ((pointer) pazSxqt_files,
				      cSxqt_files * sizeof (char *));

  pazSxqt_files[cSxqt_files - 1] = zbufcpy (argv[1]);

  return UUCONF_CMDTABRET_CONTINUE;
}

/* Get the requesting user and system from an execution file.  */

/*ARGSUSED*/
static int
isxqt_user (puuconf, argc, argv, pvar, pinfo)
     pointer puuconf;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  zSxqt_user = zbufcpy (argv[1]);
  zSxqt_system = zbufcpy (argv[2]);
  return UUCONF_CMDTABRET_CONTINUE;
}

/* Handle various possible requests to look at work files.  */

static boolean
fsworkfiles (puuconf, icmd, csystems, pazsystems, fnotsystems, cusers,
	     pazusers, fnotusers, iold, iyoung, ccommands, pazcommands,
	     fnotcommands, zcomment, cstdin)
     pointer puuconf;
     int icmd;
     int csystems;
     char **pazsystems;
     boolean fnotsystems;
     int cusers;
     char **pazusers;
     boolean fnotusers;
     long iold;
     long iyoung;
     int ccommands;
     char **pazcommands;
     boolean fnotcommands;
     const char *zcomment;
     int cstdin;
{
  boolean fret;
  int i;
  int iuuconf;
  struct uuconf_system ssys;

  fret = TRUE;

  if (csystems > 0 && ! fnotsystems)
    {
      for (i = 0; i < csystems; i++)
	{
	  iuuconf = uuconf_system_info (puuconf, pazsystems[i], &ssys);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      if (iuuconf == UUCONF_NOT_FOUND)
		ulog (LOG_ERROR, "%s: System not found", pazsystems[i]);
	      else
		ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	      fret = FALSE;
	      continue;
	    }

	  if (! fsworkfiles_system (puuconf, icmd, &ssys, cusers, pazusers,
				    fnotusers, iold, iyoung, ccommands,
				    pazcommands, fnotcommands, zcomment,
				    cstdin))
	    fret = FALSE;

	  (void) uuconf_system_free (puuconf, &ssys);
	}
    }
  else
    {
      char **pznames, **pz;

      iuuconf = uuconf_system_names (puuconf, &pznames, 0);
      if (iuuconf != UUCONF_SUCCESS)
	{
	  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	  return FALSE;
	}
      
      for (pz = pznames; *pz != NULL; pz++)
	{
	  if (csystems > 0)
	    {
	      for (i = 0; i < csystems; i++)
		if (strcmp (*pz, pazsystems[i]) == 0)
		  break;
	      if (i < csystems)
		continue;
	    }

	  iuuconf = uuconf_system_info (puuconf, *pz, &ssys);
	  if (iuuconf != UUCONF_SUCCESS)
	    {
	      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	      fret = FALSE;
	      continue;
	    }

	  if (! fsworkfiles_system (puuconf, icmd, &ssys, cusers, pazusers,
				    fnotusers, iold, iyoung, ccommands,
				    pazcommands, fnotcommands, zcomment,
				    cstdin))
	    fret = FALSE;

	  (void) uuconf_system_free (puuconf, &ssys);
	  xfree ((pointer) *pz);
	}
      xfree ((pointer) pznames);
    }

  return fret;
}

/* Look at the work files for a particular system.  */

static boolean
fsworkfiles_system (puuconf, icmd, qsys, cusers, pazusers, fnotusers, iold,
		    iyoung, ccommands, pazcommands, fnotcommands, zcomment,
		    cstdin)
     pointer puuconf;
     int icmd;
     const struct uuconf_system *qsys;
     int cusers;
     char **pazusers;
     boolean fnotusers;
     long iold;
     long iyoung;
     int ccommands;
     char **pazcommands;
     boolean fnotcommands;
     const char *zcomment;
     int cstdin;
{
  boolean fret;

  if (! fsysdep_get_work_init (qsys, UUCONF_GRADE_LOW, TRUE))
    return FALSE;

  while (TRUE)
    {
      struct scmd s;
      long itime;

      if (! fsysdep_get_work (qsys, UUCONF_GRADE_LOW, TRUE, &s))
	{
	  usysdep_get_work_free (qsys);
	  return FALSE;
	}
      if (s.bcmd == 'H')
	break;

      if (cusers > 0)
	{
	  boolean fmatch;
	  int i;

	  fmatch = fnotusers;
	  for (i = 0; i < cusers; i++)
	    {
	      if (strcmp (pazusers[i], s.zuser) == 0)
		{
		  fmatch = ! fmatch;
		  break;
		}
	    }
	  if (! fmatch)
	    continue;
	}

      itime = isysdep_work_time (qsys, s.pseq);

      if (iold != (long) -1 && itime > iold)
	continue;

      if (iyoung != (long) -1 && itime < iyoung)
	continue;

      if (! fsworkfile_show (puuconf, icmd, qsys, &s, itime, ccommands,
			     pazcommands, fnotcommands, zcomment, cstdin))
	{
	  usysdep_get_work_free (qsys);
	  return FALSE;
	}
    }

  fret = fsworkfile_show (puuconf, icmd, qsys, (const struct scmd *) NULL,
			  0L, ccommands, pazcommands, fnotcommands, zcomment,
			  cstdin);

  usysdep_get_work_free (qsys);

  return fret;
}

/* Show a single workfile.  This is actually called once for each line
   in the workfile, so we accumulate the lines and show them all at
   once.  This lets us show an execution in a useful fashion.  */

static boolean
fsworkfile_show (puuconf, icmd, qsys, qcmd, itime, ccommands, pazcommands,
		 fnotcommands, zcomment, cstdin)
     pointer puuconf;
     int icmd;
     const struct uuconf_system *qsys;
     const struct scmd *qcmd;
     long itime;
     int ccommands;
     char **pazcommands;
     boolean fnotcommands;
     const char *zcomment;
     int cstdin;
{
  static struct scmdlist *qlist;
  static char *zlistid;
  char *zid;

  if (qcmd == NULL)
    zid = NULL;
  else
    {
      zid = zsysdep_jobid (qsys, qcmd->pseq);
      if (zid == NULL)
	return FALSE;
    }

  /* If this is the same jobid as the list, put it on the end.  */

  if (qcmd != NULL
      && qlist != NULL
      && strcmp (zlistid, zid) == 0)
    {
      struct scmdlist *qnew, **pq;

      ubuffree (zid);
      qnew = (struct scmdlist *) xmalloc (sizeof (struct scmdlist));
      qnew->qnext = NULL;
      qnew->s = *qcmd;
      qnew->itime = itime;
      for (pq = &qlist; *pq != NULL; pq = &(*pq)->qnext)
	;
      *pq = qnew;
      return TRUE;
    }

  /* Here we have found a different job ID, so we print the scmd
     structures that we have accumulated.  We look for the special
     case of an execution (one of the destination files begins with
     X.).  We could be more clever about other situations as well.  */
  if (qlist != NULL)
    {
      boolean fmatch;
      const char *zstdin;
      struct scmdlist *qxqt;
      struct scmdlist *qfree;

      fmatch = FALSE;
      zstdin = NULL;

      for (qxqt = qlist; qxqt != NULL; qxqt = qxqt->qnext)
	if (qxqt->s.bcmd == 'S'
	    && qxqt->s.zto[0] == 'X'
	    && qxqt->s.zto[1] == '.'
	    && fspool_file (qxqt->s.zfrom))
	  break;

      if (qxqt == NULL)
	{
	  zSxqt_cmd = NULL;
	  zSxqt_requestor = NULL;
	  if (ccommands == 0
	      || (fnotcommands
		  && strcmp (pazcommands[0], "ALL") == 0))
	    {
	      /* Show all the lines in a regular work file.  */

	      fmatch = TRUE;

	      if ((icmd & JOB_SHOW) != 0)
		{
		  struct scmdlist *qshow;

		  for (qshow = qlist; qshow != NULL; qshow = qshow->qnext)
		    {
		      char *zfile;

		      usworkfile_header (qsys, &qshow->s, zlistid,
					 qshow->itime, qshow == qlist);

		      switch (qshow->s.bcmd)
			{
			case 'S':
			  if (strchr (qshow->s.zoptions, 'C') != NULL
			      || fspool_file (qshow->s.zfrom))
			    zfile = zsysdep_spool_file_name (qsys,
							     qshow->s.ztemp);
			  else
			    zfile = zbufcpy (qshow->s.zfrom);
			  printf ("Sending %s (%ld bytes) to %s",
				  qshow->s.zfrom,
				  zfile == NULL ? 0L : csysdep_size (zfile),
				  qshow->s.zto);
			  ubuffree (zfile);
			  break;
			case 'R':
			  printf ("Requesting %s to %s", qshow->s.zfrom,
				  qshow->s.zto);
			  break;
			case 'X':
			  printf ("Requesting %s to %s", qshow->s.zfrom,
				  qshow->s.zto);
			  break;
#if DEBUG > 0
			default:
			  printf ("Bad line %d", qshow->s.bcmd);
			  break;
#endif
			}

		      printf ("\n");
		    }
		}
	    }
	}
      else
	{
	  char *zxqt;
	  long csize;
	  struct scmdlist *qsize;

	  /* Show the command for an execution file.  */
	  zxqt = zsysdep_spool_file_name (qsys, qxqt->s.zfrom);
	  if (zxqt == NULL)
	    return FALSE;

	  if (! fsxqt_file_read (puuconf, zxqt))
	    {
	      ubuffree (zxqt);
	      return FALSE;
	    }

	  ubuffree (zxqt);

	  csize = 0L;
	  for (qsize = qlist; qsize != NULL; qsize = qsize->qnext)
	    {
	      if (qsize->s.bcmd == 'S')
		{
		  char *zfile;

		  if (strchr (qsize->s.zoptions, 'C') != NULL
		      || fspool_file (qsize->s.zfrom))
		    zfile = zsysdep_spool_file_name (qsys, qsize->s.ztemp);
		  else
		    zfile = zbufcpy (qsize->s.zfrom);
		  if (zfile != NULL)
		    {
		      csize += csysdep_size (zfile);
		      ubuffree (zfile);
		    }
		}
	    }

	  if (ccommands == 0)
	    fmatch = TRUE;
	  else
	    {
	      int i;

	      fmatch = fnotcommands;
	      for (i = 0; i < ccommands; i++)
		{
		  if (strcmp (pazcommands[i], "ALL") == 0
		      || strcmp (pazcommands[i], zSxqt_prog) == 0)
		    {
		      fmatch = ! fmatch;
		      break;
		    }
		}
	    }

	  /* To get the name of the standard input file on this system
	     we have to look through the list of file transfers to
	     find the right one on the remote system.  */
	  if (fmatch)
	    {
	      struct scmdlist *qstdin;

	      for (qstdin = qlist; qstdin != NULL; qstdin = qstdin->qnext)
		{
		  if (qstdin->s.bcmd == 'S'
		      && strcmp (qstdin->s.zto, zSxqt_stdin) == 0)
		    {
		      if (strchr (qstdin->s.zoptions, 'C') != NULL
			  || fspool_file (qstdin->s.zfrom))
			zstdin = qstdin->s.ztemp;
		      else
			zstdin = qstdin->s.zfrom;
		      break;
		    }
		}
	    }

	  if (fmatch && (icmd & JOB_SHOW) != 0)
	    {
	      usworkfile_header (qsys, &qxqt->s, zlistid, qxqt->itime,
				 TRUE);
	      printf ("Executing %s (sending %ld bytes)\n",
		      zSxqt_cmd, csize);
	    }
	}

      if (fmatch)
	{
	  boolean fkill;

	  fkill = FALSE;
	  if ((icmd & JOB_INQUIRE) != 0)
	    {
	      int b;

	      /* Ask stdin whether this job should be killed.  */
	      fprintf (stderr, "%s: Kill %s? ", abProgram, zlistid);
	      (void) fflush (stderr);
	      b = getchar ();
	      fkill = b == 'y' || b == 'Y';
	      while (b != EOF && b != '\n')
		b = getchar ();
	    }
	  else if ((icmd & JOB_KILL) != 0)
	    fkill = TRUE;
	      
	  if (fkill
	      && strcmp (zsysdep_login_name (), qlist->s.zuser) != 0
	      && ! fsysdep_privileged ())
	    ulog (LOG_ERROR, "%s: Not submitted by you", zlistid);
	  else
	    {
	      if ((icmd & (JOB_MAIL | JOB_NOTIFY)) != 0)
		{
		  if (! fsnotify (puuconf, icmd, zcomment, cstdin, fkill,
				  zSxqt_cmd, qlist, zlistid, qlist->s.zuser,
				  qsys, zstdin, zSxqt_requestor))
		    return FALSE;
		}

	      if (fkill)
		{
		  if (! fsysdep_kill_job (puuconf, zlistid))
		    return FALSE;
		}
	    }
	}

      if (qxqt != NULL)
	usxqt_file_free ();

      /* Free up the list of entries.  */
      qfree = qlist;
      while (qfree != NULL)
	{
	  struct scmdlist *qnext;

	  qnext = qfree->qnext;
	  xfree ((pointer) qfree);
	  qfree = qnext;
	}

      ubuffree (zlistid);

      qlist = NULL;
      zlistid = NULL;
    }

  /* Start a new list with the entry we just got.  */

  if (qcmd != NULL)
    {
      qlist = (struct scmdlist *) xmalloc (sizeof (struct scmdlist));
      qlist->qnext = NULL;
      qlist->s = *qcmd;
      qlist->itime = itime;
      zlistid = zid;
    }

  return TRUE;
}

/* Show the header of the line describing a workfile.  */

static void
usworkfile_header (qsys, qcmd, zjobid, itime, ffirst)
     const struct uuconf_system *qsys;
     const struct scmd *qcmd;
     const char *zjobid;
     long itime;
     boolean ffirst;
{
  const char *zshowid;
  struct tm stime;

  if (ffirst)
    zshowid = zjobid;
  else
    zshowid = "-";

  printf ("%s %s %s ", zshowid, qsys->uuconf_zname, qcmd->zuser);

  usysdep_localtime (itime, &stime);
  printf ("%04d-%02d-%02d %02d:%02d:%02d ",
	  stime.tm_year + 1900, stime.tm_mon + 1,
	  stime.tm_mday, stime.tm_hour,
	  stime.tm_min, stime.tm_sec);
}

/* List queued executions that have not been processed by uuxqt for
   one reason or another.  */

static boolean
fsexecutions (puuconf, icmd, csystems, pazsystems, fnotsystems, cusers,
	      pazusers, fnotusers, iold, iyoung, ccommands, pazcommands,
	      fnotcommands, zcomment, cstdin)
     pointer puuconf;
     int icmd;
     int csystems;
     char **pazsystems;
     boolean fnotsystems;
     int cusers;
     char **pazusers;
     boolean fnotusers;
     long iold;
     long iyoung;
     int ccommands;
     char **pazcommands;
     boolean fnotcommands;
     const char *zcomment;
     int cstdin;
{
  char *zfile;
  char *zsystem;
  boolean ferr;

  if (! fsysdep_get_xqt_init ())
    return FALSE;

  while ((zfile = zsysdep_get_xqt (&zsystem, &ferr)) != NULL)
    {
      boolean fmatch;
      int i;
      long itime;

      if (csystems > 0)
	{
	  fmatch = fnotsystems;
	  for (i = 0; i < csystems; i++)
	    {
	      if (strcmp (pazsystems[i], zsystem) == 0)
		{
		  fmatch = ! fmatch;
		  break;
		}
	    }
	  if (! fmatch)
	    {
	      ubuffree (zfile);
	      ubuffree (zsystem);
	      continue;
	    }
	}

      itime = isysdep_file_time (zfile);

      if ((iold != (long) -1 && itime > iold)
	  || (iyoung != (long) -1 && itime < iyoung))
	{
	  ubuffree (zfile);
	  ubuffree (zsystem);
	  continue;
	}

      /* We need to read the execution file before we can check the
	 user name.  */
      if (! fsxqt_file_read (puuconf, zfile))
	{
	  ubuffree (zfile);
	  ubuffree (zsystem);
	  continue;      
	}

      if (cusers == 0)
	fmatch = TRUE;
      else
	{
	  fmatch = fnotusers;
	  for (i = 0; i < cusers; i++)
	    {
	      if (strcmp (zSxqt_user, pazusers[i]) == 0
		  || (zSxqt_requestor != NULL
		      && strcmp (zSxqt_requestor, pazusers[i]) == 0))
		{
		  fmatch = ! fmatch;
		  break;
		}
	    }
	}

      if (fmatch && ccommands > 0)
	{
	  fmatch = fnotcommands;
	  for (i = 0; i < ccommands; i++)
	    {
	      if (strcmp (pazcommands[i], "ALL") == 0
		  || strcmp (pazcommands[i], zSxqt_prog) == 0)
		{
		  fmatch = ! fmatch;
		  break;
		}
	    }
	}

      if (fmatch)
	{
	  boolean fbad, fkill;
	  struct uuconf_system ssys;
	  int iuuconf;

	  fbad = FALSE;

	  if ((icmd & JOB_SHOW) != 0)
	    {
	      struct tm stime;

	      printf ("%s %s!", zsystem, zSxqt_system);
	      if (zSxqt_requestor != NULL)
		printf ("%s", zSxqt_requestor);
	      else
		printf ("%s", zSxqt_user);

	      usysdep_localtime (itime, &stime);
	      printf (" %04d-%02d-%02d %02d:%02d:%02d ",
		      stime.tm_year + 1900, stime.tm_mon + 1,
		      stime.tm_mday, stime.tm_hour,
		      stime.tm_min, stime.tm_sec);

	      printf ("%s\n", zSxqt_cmd);
	    }

	  fkill = FALSE;
	  if ((icmd & JOB_INQUIRE) != 0)
	    {
	      int b;

	      /* Ask stdin whether this job should be killed.  */
	      fprintf (stderr, "%s: Kill %s? ", abProgram, zSxqt_cmd);
	      (void) fflush (stderr);
	      b = getchar ();
	      fkill = b == 'y' || b == 'Y';
	      while (b != EOF && b != '\n')
		b = getchar ();
	    }
	  else if ((icmd & JOB_KILL) != 0)
	    fkill = TRUE;

	  if (fkill)
	    {
	      const char *zlocalname;

	      if (strcmp (zSxqt_user, zsysdep_login_name ()) != 0
		  && ! fsysdep_privileged ())
		{
		  ulog (LOG_ERROR, "Job not submitted by you\n");
		  fbad = TRUE;
		}

	      if (! fbad)
		{
		  iuuconf = uuconf_localname (puuconf, &zlocalname);
		  if (iuuconf == UUCONF_NOT_FOUND)
		    {
		      zlocalname = zsysdep_localname ();
		      if (zlocalname == NULL)
			fbad = TRUE;
		    }
		  else if (iuuconf != UUCONF_SUCCESS)
		    {
		      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		      fbad = TRUE;
		    }
		}

	      if (! fbad && strcmp (zsystem, zlocalname) != 0)
		{
		  ulog (LOG_ERROR, "Job not submitted by you");
		  fbad = TRUE;
		}
	    }

	  if (! fbad)
	    {
	      iuuconf = uuconf_system_info (puuconf, zsystem, &ssys);
	      if (iuuconf != UUCONF_SUCCESS)
		{
		  if (iuuconf != UUCONF_NOT_FOUND)
		    {
		      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		      fbad = TRUE;
		    }
		  else if (! funknown_system (puuconf, zsystem, &ssys))
		    {
		      ulog (LOG_ERROR, "Job for unknown system %s",
			    zsystem);
		      fbad = TRUE;
		    }
		}
	    }

	  if (! fbad && (icmd & (JOB_MAIL | JOB_NOTIFY)) != 0)
	    {
	      if (! fsnotify (puuconf, icmd, zcomment, cstdin, fkill,
			      zSxqt_cmd, (struct scmdlist *) NULL,
			      (const char *) NULL, zSxqt_user, &ssys,
			      zSxqt_stdin, zSxqt_requestor))
		{
		  ferr = TRUE;
		  usxqt_file_free ();
		  ubuffree (zfile);
		  ubuffree (zsystem);
		  break;
		}
	    }

	  if (! fbad && fkill)
	    {
	      for (i = 0; i < cSxqt_files; i++)
		{
		  char *z;

		  z = zsysdep_spool_file_name (&ssys, pazSxqt_files[i]);
		  if (z != NULL)
		    {
		      (void) remove (z);
		      ubuffree (z);
		    }
		}
	      if (remove (zfile) != 0)
		ulog (LOG_ERROR, "remove (%s): %s", zfile,
		      strerror (errno));
	    }

	  if (! fbad)
	    (void) uuconf_system_free (puuconf, &ssys);
	}

      usxqt_file_free ();
      ubuffree (zfile);
      ubuffree (zsystem);
    }

  usysdep_get_xqt_free ();

  return ferr;
}

/* When a job is killed, send mail to the appropriate people.  */

static boolean
fsnotify (puuconf, icmd, zcomment, cstdin, fkilled, zcmd, qcmd, zid, zuser,
	  qsys, zstdin, zrequestor)
     pointer puuconf;
     int icmd;
     const char *zcomment;
     int cstdin;
     boolean fkilled;
     const char *zcmd;
     struct scmdlist *qcmd;
     const char *zid;
     const char *zuser;
     const struct uuconf_system *qsys;
     const char *zstdin;
     const char *zrequestor;
{
  const char **pz;
  int cgot;
  int i, istdin;
  const char *zsubject;
  boolean fret;

  pz = (const char **) xmalloc (20 * sizeof (const char *));
  cgot = 20;

  i = 0;
  if (zid == NULL)
    pz[i++] = "A UUCP execution request\n";
  else
    {
      pz[i++] = "UUCP job\n\t";
      pz[i++] = zid;
      pz[i++] = "\n";
    }
  pz[i++] = "requested by\n\t";
  pz[i++] = zuser;
  pz[i++] = "\non system\n\t";
  pz[i++] = qsys->uuconf_zname;
  pz[i++] = "\n";

  if (fkilled)
    pz[i++] = "has been killed.\n";

  if (zcomment != NULL)
    {
      pz[i++] = zcomment;
      pz[i++] = "\n";
    }

  pz[i++] = "The job ";
  if (fkilled)
    pz[i++] = "was\n";
  else
    pz[i++] = "is\n";

  if (zcmd != NULL)
    {
      pz[i++] = "\t";
      pz[i++] = zcmd;
    }
  else
    {
      struct scmdlist *qshow;

      for (qshow = qcmd; qshow != NULL; qshow = qshow->qnext)
	{
	  if (i + 10 > cgot)
	    {
	      cgot += 20;
	      pz = (const char **) xrealloc ((pointer) pz,
					     cgot * sizeof (const char *));
	    }

	  switch (qshow->s.bcmd)
	    {
	    case 'S':
	      pz[i++] = "\tsend ";
	      break;
	    default:
	    case 'R':
	    case 'X':
	      pz[i++] = "\trequest ";
	      break;
	    }
	  pz[i++] = qshow->s.zfrom;
	  pz[i++] = " to ";
	  pz[i++] = qshow->s.zto;
	}
    }

  istdin = i;
  if (cstdin > 0 && zstdin != NULL)
    {
      boolean fspool;
      char *zfile;
      FILE *e;

      fspool = fspool_file (zstdin);
      if (fspool)
	zfile = zsysdep_spool_file_name (qsys, zstdin);
      else
	zfile = zsysdep_local_file (zstdin, qsys->uuconf_zpubdir);

      if (zfile != NULL
	  && (fspool
	      || fin_directory_list (zfile, qsys->uuconf_pzremote_send,
				     qsys->uuconf_zpubdir, TRUE, TRUE,
				     (const char *) NULL)))
	{
	  e = fopen (zfile, "r");
	  if (e != NULL)
	    {
	      int clines;
	      char *zline;
	      size_t cline;

	      pz[i++] = "\n";
	      istdin = i;

	      clines = 0;

	      zline = NULL;
	      cline = 0;
	      while (getline (&zline, &cline, e) > 0)
		{
		  ++clines;
		  if (clines > cstdin)
		    break;
		  if (i >= cgot)
		    {
		      cgot += 20;
		      pz = (const char **) xrealloc ((pointer) pz,
						     (cgot
						      * sizeof (char *)));
		    }
		  pz[i++] = zbufcpy (zline);
		}
	      xfree ((pointer) zline);
	      (void) fclose (e);
	    }
	}

      ubuffree (zfile);
    }

  if (fkilled)
    zsubject = "UUCP job killed";
  else
    zsubject = "UUCP notification";

  fret = TRUE;

  if ((icmd & JOB_MAIL) != 0)
    {
      if (! fsysdep_mail (OWNER, zsubject, i, pz))
	fret = FALSE;
    }

  if ((icmd & JOB_NOTIFY) != 0)
    {
      const char *zmail;
      int iuuconf;
      const char *zloc;

      if (zrequestor != NULL)
	zmail = zrequestor;
      else
	zmail = zuser;

      iuuconf = uuconf_localname (puuconf, &zloc);
      if (iuuconf == UUCONF_NOT_FOUND)
	{
	  zloc = zsysdep_localname ();
	  if (zloc == NULL)
	    return FALSE;
	}
      else if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

      if (strcmp (qsys->uuconf_zname, zloc) != 0
#if HAVE_INTERNET_MAIL
	  && strchr (zmail, '@') == NULL
#endif
	  )
	{
	  char *zset;

	  zset = (char *) alloca (strlen (qsys->uuconf_zname)
				  + strlen (zmail)
				  + sizeof "!");
	  sprintf (zset, "%s!%s", qsys->uuconf_zname, zmail);
	  zmail = zset;
	}

      xfree ((pointer) zloc);

      if (! fsysdep_mail (zmail, zsubject, i, pz))
	fret = FALSE;
    }

  while (istdin < i)
    {
      ubuffree ((char *) pz[istdin]);
      istdin++;
    }

  xfree ((pointer) pz);

  return fret;
}

/* Handle the -q option.  For each remote system this lists the number
   of jobs queued, the number of executions queued, and the current
   call status.  We get the executions all at once, because they are
   not accessed by system.  They could be, but it is possible to have
   executions pending for an unknown system, so special handling would
   still be required.  */

struct sxqtlist
{
  struct sxqtlist *qnext;
  char *zsystem;
  int cxqts;
  long ifirst;
};

/* These local functions need the definition of sxqtlist for the
   prototype.  */

static boolean fsquery_system P((const struct uuconf_system *qsys,
				 struct sxqtlist **pq,
				 long inow, const char *zlocalname));
static boolean fsquery_show P((const struct uuconf_system *qsys, int cwork,
			       long ifirstwork,
			       struct sxqtlist *qxqt,
			       long inow, const char *zlocalname));

static boolean
fsquery (puuconf)
     pointer puuconf;
{
  int iuuconf;
  const char *zlocalname;
  struct sxqtlist *qlist;
  char *zfile, *zsystem;
  boolean ferr;
  long inow;
  char **pznames, **pz;
  boolean fret;

  iuuconf = uuconf_localname (puuconf, &zlocalname);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      zlocalname = zsysdep_localname ();
      if (zlocalname == NULL)
	return FALSE;
    }
  else if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return FALSE;
    }

  /* Get a count of all the execution files.  */
  if (! fsysdep_get_xqt_init ())
    return FALSE;

  qlist = NULL;
  while ((zfile = zsysdep_get_xqt (&zsystem, &ferr)) != NULL)
    {
      struct sxqtlist *qlook;

      for (qlook = qlist; qlook != NULL; qlook = qlook->qnext)
	if (strcmp (zsystem, qlook->zsystem) == 0)
	  break;

      if (qlook != NULL)
	{
	  long itime;

	  ubuffree (zsystem);
	  ++qlook->cxqts;
	  itime = isysdep_file_time (zfile);
	  if (itime < qlook->ifirst)
	    qlook->ifirst = itime;
	}
      else
	{
	  struct sxqtlist *qnew;

	  qnew = (struct sxqtlist *) xmalloc (sizeof (struct sxqtlist));
	  qnew->qnext = qlist;
	  qnew->zsystem = zsystem;
	  qnew->cxqts = 1;
	  qnew->ifirst = isysdep_file_time (zfile);
	  qlist = qnew;
	}

      ubuffree (zfile);
    }

  usysdep_get_xqt_free ();

  if (ferr)
    return FALSE;

  inow = isysdep_time ((long *) NULL);

  /* Show the information for each system.  */
  iuuconf = uuconf_system_names (puuconf, &pznames, 0);
  if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return FALSE;
    }

  fret = TRUE;

  for (pz = pznames; *pz != NULL; pz++)
    {
      struct uuconf_system ssys;

      iuuconf = uuconf_system_info (puuconf, *pz, &ssys);
      if (iuuconf != UUCONF_SUCCESS)
	{
	  ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
	  fret = FALSE;
	  continue;
	}

      if (! fsquery_system (&ssys, &qlist, inow, zlocalname))
	fret = FALSE;

      (void) uuconf_system_free (puuconf, &ssys);
      xfree ((pointer) *pz);
    }

  /* Check for the local system in the list of execution files.  */
  if (qlist != NULL)
    {
      struct sxqtlist **pq;

      for (pq = &qlist; *pq != NULL; pq = &(*pq)->qnext)
	{
	  if (strcmp ((*pq)->zsystem, zlocalname) == 0)
	    {
	      struct uuconf_system ssys;
	      struct sxqtlist *qfree;

	      iuuconf = uuconf_system_info (puuconf, zlocalname, &ssys);
	      if (iuuconf != UUCONF_SUCCESS)
		{
		  if (iuuconf != UUCONF_NOT_FOUND)
		    {
		      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		      fret = FALSE;
		      break;
		    }

		  iuuconf = uuconf_system_local (puuconf, &ssys);
		  if (iuuconf != UUCONF_SUCCESS)
		    {
		      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
		      fret = FALSE;
		      break;
		    }
		  ssys.uuconf_zname = (char *) zlocalname;
		}

	      if (! fsquery_show (&ssys, 0, 0L, *pq, inow, zlocalname))
		fret = FALSE;
	      (void) uuconf_system_free (puuconf, &ssys);
	      qfree = *pq;
	      *pq = qfree->qnext;
	      ubuffree (qfree->zsystem);
	      xfree ((pointer) qfree);
	      break;
	    }
	}
    }

  /* Print out information for any unknown systems for which we have
     execution files.  */
  while (qlist != NULL)
    {
      struct uuconf_system ssys;
      struct sxqtlist *qnext;

      if (! funknown_system (puuconf, qlist->zsystem, &ssys))
	{
	  ulog (LOG_ERROR, "Executions queued up for unknown systems");
	  fret = FALSE;
	  break;
	}

      if (! fsquery_show (&ssys, 0, 0L, qlist, inow, zlocalname))
	fret = FALSE;
      (void) uuconf_system_free (puuconf, &ssys);
      qnext = qlist->qnext;
      ubuffree (qlist->zsystem);
      xfree ((pointer) qlist);
      qlist = qnext;
    }

  return fret;
}

/* Query a single known system.  */

static boolean
fsquery_system (qsys, pq, inow, zlocalname)
     const struct uuconf_system *qsys;
     struct sxqtlist **pq;
     long inow;
     const char *zlocalname;
{
  int cwork;
  long ifirstwork;
  char *zid;
  boolean fret;

  if (! fsysdep_get_work_init (qsys, UUCONF_GRADE_LOW, TRUE))
    return FALSE;

  cwork = 0;
  ifirstwork = 0L;
  zid = NULL;
  while (TRUE)
    {
      struct scmd s;
      long itime;
      char *zthisid;

      if (! fsysdep_get_work (qsys, UUCONF_GRADE_LOW, TRUE, &s))
	return FALSE;
      if (s.bcmd == 'H')
	break;

      zthisid = zsysdep_jobid (qsys, s.pseq);
      if (zid != NULL && strcmp (zid, zthisid) == 0)
	ubuffree (zthisid);
      else
	{
	  ++cwork;
	  ubuffree (zid);
	  zid = zthisid;
	}

      itime = isysdep_work_time (qsys, s.pseq);
      if (ifirstwork == 0L || ifirstwork > itime)
	ifirstwork = itime;
    }

  usysdep_get_work_free (qsys);
  ubuffree (zid);

  /* Find the execution information, if any.  */
  while (*pq != NULL)
    {
      if (strcmp ((*pq)->zsystem, qsys->uuconf_zname) == 0)
	break;
      pq = &(*pq)->qnext;
    }

  /* If there are no commands and no executions, don't print any
     information for this system.  */
  if (cwork == 0 && *pq == NULL)
    return TRUE;

  fret = fsquery_show (qsys, cwork, ifirstwork, *pq, inow, zlocalname);

  if (*pq != NULL)
    {
      struct sxqtlist *qfree;

      qfree = *pq;
      *pq = qfree->qnext;
      ubuffree (qfree->zsystem);
      xfree ((pointer) qfree);
    }

  return fret;
}

/* Print out the query information for a single system.  We handle the
   local system specially.  */

static boolean
fsquery_show (qsys, cwork, ifirstwork, qxqt, inow, zlocalname)
     const struct uuconf_system *qsys;
     int cwork;
     long ifirstwork;
     struct sxqtlist *qxqt;
     long inow;
     const char *zlocalname;
{
  boolean flocal;
  struct sstatus sstat;
  boolean fnostatus;
  struct tm stime;

  flocal = strcmp (qsys->uuconf_zname, zlocalname) == 0;

  if (! flocal)
    {
      if (! fsysdep_get_status (qsys, &sstat, &fnostatus))
	return FALSE;
    }

  printf ("%s %dC (", qsys->uuconf_zname, cwork);

  if (cwork == 0)
    printf ("0 secs");
  else
    usunits_show (inow - ifirstwork);

  printf (") ");

  if (qxqt == NULL)
    printf ("0X (0 secs)");
  else
    {
      printf ("%dX (", qxqt->cxqts);
      usunits_show (inow - qxqt->ifirst);
      printf (")");
    }

  if (flocal || fnostatus)
    {
      printf ("\n");
      return TRUE;
    }

  usysdep_localtime (sstat.ilast, &stime);

  printf (" %04d-%02d-%02d %02d:%02d:%02d ", 
	  stime.tm_year + 1900, stime.tm_mon + 1,
	  stime.tm_mday, stime.tm_hour,
	  stime.tm_min, stime.tm_sec);

  printf ("%s\n", azStatus[(int) sstat.ttype]);

  return TRUE;
}

/* Print a time difference in the largest applicable units.  */

static void
usunits_show (idiff)
     long idiff;
{
  const char *zunit;
  long iunits;

  if (idiff > (long) 24 * (long) 60 * (long) 60)
    {
      iunits = idiff / ((long) 24 * (long) 60 * (long) 60);
      zunit = "day";
    }
  else if (idiff > (long) 60 * 60)
    {
      iunits = idiff / (long) (60 * 60);
      zunit = "hour";
    }
  else if (idiff > (long) 60)
    {
      iunits = idiff / (long) 60;
      zunit = "min";
    }
  else
    {
      iunits = idiff;
      zunit = "sec";
    }

  printf ("%ld %s%s", iunits, zunit, iunits == 1 ? "" : "s");
}

/* Give a list of all status entries for all machines that we have
   status entries for.  We need to get a list of status entries in a
   system dependent fashion, since we may have status for unknown
   systems.  */

static boolean
fsmachines ()
{
  pointer phold;
  char *zsystem;
  boolean ferr;
  struct sstatus sstat;

  if (! fsysdep_all_status_init (&phold))
    return FALSE;

  while ((zsystem = zsysdep_all_status (phold, &ferr, &sstat)) != NULL)
    {
      struct tm stime;

      usysdep_localtime (sstat.ilast, &stime);
      printf ("%-14s %04d-%02d-%02d %02d:%02d:%02d %s", zsystem,
	      stime.tm_year + 1900, stime.tm_mon + 1,
	      stime.tm_mday, stime.tm_hour,
	      stime.tm_min, stime.tm_sec,
	      azStatus[(int) sstat.ttype]);
      ubuffree (zsystem);
      if (sstat.ttype != STATUS_TALKING
	  && sstat.cwait > 0)
	{
	  printf (" (%d %s", sstat.cretries,
		  sstat.cretries == 1 ? "try" : "tries");
	  if (sstat.ilast + sstat.cwait > isysdep_time ((long *) NULL))
	    {
	      usysdep_localtime (sstat.ilast + sstat.cwait, &stime);
	      printf (", next %04d-%02d-%02d %02d:%02d:%02d",
		      stime.tm_year + 1900, stime.tm_mon + 1,
		      stime.tm_mday, stime.tm_hour,
		      stime.tm_min, stime.tm_sec);
	    }
	  printf (")");
	}
      printf ("\n");
    }

  usysdep_all_status_free (phold);

  return ! ferr;
}
