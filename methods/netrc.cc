// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: netrc.c,v 1.38 2007-11-07 09:21:35 bagder Exp $
/* ######################################################################

   netrc file parser - returns the login and password of a give host in
                       a specified netrc-type file

   Originally written by Daniel Stenberg, <daniel@haxx.se>, et al. and
   placed into the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "netrc.h"

/* Debug this single source file with:
   'make netrc' then run './netrc'!

   Oh, make sure you have a .netrc file too ;-)
 */

/* Get user and password from .netrc when given a machine name */

enum {
  NOTHING,
  HOSTFOUND,    /* the 'machine' keyword was found */
  HOSTCOMPLETE, /* the machine name following the keyword was found too */
  HOSTVALID,    /* this is "our" machine! */
  HOSTEND /* LAST enum */
};

/* make sure we have room for at least this size: */
#define LOGINSIZE 64
#define PASSWORDSIZE 64
#define NETRC DOT_CHAR "netrc"

/* returns -1 on failure, 0 if the host is found, 1 is the host isn't found */
int parsenetrc (char *host, char *login, char *password, char *netrcfile = NULL)
{
  FILE *file;
  int retcode = 1;
  int specific_login = (login[0] != 0);
  char *home = NULL;
  bool netrc_alloc = false;
  int state = NOTHING;

  char state_login = 0;        /* Found a login keyword */
  char state_password = 0;     /* Found a password keyword */
  int state_our_login = false;  /* With specific_login,
                                   found *our* login name */

  if (!netrcfile) {
    home = getenv ("HOME"); /* portable environment reader */

    if (!home) {
      struct passwd *pw;
      pw = getpwuid (geteuid ());
      if(pw)
        home = pw->pw_dir;
    }

    if (!home)
      return -1;

    asprintf (&netrcfile, "%s%s%s", home, DIR_CHAR, NETRC);
    if(!netrcfile)
      return -1;
    else
      netrc_alloc = true;
  }

  file = fopen (netrcfile, "r");
  if(file) {
    char *tok;
    char *tok_buf;
    bool done = false;
    char netrcbuffer[256];

    while (!done && fgets(netrcbuffer, sizeof (netrcbuffer), file)) {
      tok = strtok_r (netrcbuffer, " \t\n", &tok_buf);
      while (!done && tok) {
        if(login[0] && password[0]) {
          done = true;
          break;
        }

        switch(state) {
        case NOTHING:
          if (!strcasecmp ("machine", tok)) {
            /* the next tok is the machine name, this is in itself the
               delimiter that starts the stuff entered for this machine,
               after this we need to search for 'login' and
               'password'. */
            state = HOSTFOUND;
          }
          break;
        case HOSTFOUND:
          if (!strcasecmp (host, tok)) {
            /* and yes, this is our host! */
            state = HOSTVALID;
            retcode = 0; /* we did find our host */
          }
          else
            /* not our host */
            state = NOTHING;
          break;
        case HOSTVALID:
          /* we are now parsing sub-keywords concerning "our" host */
          if (state_login) {
            if (specific_login)
              state_our_login = !strcasecmp (login, tok);
            else
              strncpy (login, tok, LOGINSIZE - 1);
            state_login = 0;
          } else if (state_password) {
            if (state_our_login || !specific_login)
              strncpy (password, tok, PASSWORDSIZE - 1);
            state_password = 0;
          } else if (!strcasecmp ("login", tok))
            state_login = 1;
          else if (!strcasecmp ("password", tok))
            state_password = 1;
          else if(!strcasecmp ("machine", tok)) {
            /* ok, there's machine here go => */
            state = HOSTFOUND;
            state_our_login = false;
          }
          break;
        } /* switch (state) */

        tok = strtok_r (NULL, " \t\n", &tok_buf);
      } /* while(tok) */
    } /* while fgets() */

    fclose(file);
  }

  if (netrc_alloc)
    free(netrcfile);

  return retcode;
}

static const char *
get_osso_product_hardware ()
{
  static char *product_hardware = NULL;

  if (product_hardware)
    return product_hardware;

  /* XXX - There is a library in maemo somewhere to do this, but it is
           not included in the maemo SDK, so we have to do it
           ourselves.  Ridiculous, I know.
  */

  FILE *f = fopen ("/proc/component_version", "r");
  if (f)
    {
      char *line = NULL;
      size_t len = 0;
      ssize_t n;

      while ((n = getline (&line, &len, f)) != -1)
        {
          if (n > 0 && line[n-1] == '\n')
            line[n-1] = '\0';

          if (sscanf (line, "product %as", &product_hardware) == 1)
            break;
        }

      free (line);
      fclose (f);
    }

  return product_hardware;
}

void maybe_add_auth (URI &Uri, string NetRCFile)
{
  if (Uri.Password.empty () == true && Uri.User.empty () == true)
  {
    if (NetRCFile.empty () == false)
    {
      char login[64] = "";
      char password[64] = "";
      char *netrcfile = strdup (NetRCFile.c_str ());
      char *host = strdup (Uri.Host.c_str ());

      if (host && 0 == parsenetrc (host, login, password, netrcfile))
      {
        Uri.User = string (login);
        Uri.Password = string (password);
      }

      if (host)
        free (host);
      free (netrcfile);
    }

    const char *product_hardware = get_osso_product_hardware ();
    if (product_hardware && product_hardware[0] != '\0'
        && Uri.Password.empty () == true && Uri.User.empty () == true)
    {
      Uri.User = string ("NOKIA-OSSO-") + string (product_hardware);
      Uri.Password = string ("JOSHUA");
    }
  }
}

#ifdef DEBUG
int main(int argc, char* argv[])
{
  char login[64] = "";
  char password[64] = "";

  if(argc < 2)
    return -1;

  if(0 == parsenetrc (argv[1], login, password, argv[2])) {
    printf("HOST: %s LOGIN: %s PASSWORD: %s\n", argv[1], login, password);
  }
}
#endif