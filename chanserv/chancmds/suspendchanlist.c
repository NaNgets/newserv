/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: suspendchanlist
 * CMDLEVEL: QCMD_HELPER
 * CMDARGS: 1
 * CMDDESC: Lists suspended channels.
 * CMDFUNC: csc_dosuspendchanlist
 * CMDPROTO: int csc_dosuspendchanlist(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: suspendchanlist <pattern>
 * CMDHELP: Lists all suspended channels that match the specified pattern.
 */

#include "../chanserv.h"
#include "../../nick/nick.h"
#include "../../lib/flags.h"
#include "../../lib/irc_string.h"
#include "../../channel/channel.h"
#include "../../parser/parser.h"
#include "../../irc/irc.h"
#include "../../localuser/localuserchannel.h"
#include <string.h>
#include <stdio.h>

int csc_dosuspendchanlist(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;
  int i, seewhom;
  char *bywhom, buf[200];
  unsigned int count=0;
  struct tm *tmp;
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspendchanlist");
    return CMD_ERROR;
  }
  
  seewhom = cs_privcheck(QPRIV_VIEWSUSPENDEDBY, sender);
  if(!seewhom)
    bywhom = "(hidden)";

  chanservstdmessage(sender, QM_SUSPENDCHANLISTHEADER);
  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if (!(rcp=(regchan*)cip->exts[chanservext]))
        continue;
      
      if (!CIsSuspended(rcp))
        continue;
      
      if ((rcp->suspendby != rup->ID) && match(cargv[0], cip->name->content))
        continue;

      if(seewhom) {      
        if (rcp->suspendby == rup->ID)
          bywhom=rup->username;
        else {
          reguser *trup=findreguserbyID(rcp->suspendby);
          if (trup)
            bywhom=trup->username;
          else
            bywhom="(unknown)";
        }
      }
      count++;

      tmp=gmtime(&(rcp->suspendtime));
      strftime(buf,sizeof(buf),Q9_FORMAT_TIME,tmp);

      chanservsendmessage(sender, "%-30s %-15s %-15s %s", cip->name->content, bywhom, buf, rcp->suspendreason?rcp->suspendreason->content:"(no reason)");
      if (count >= 2000) {
        chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "channels");
        return CMD_ERROR;
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "channel", (count==1)?"":"s");
  
  return CMD_OK;
}
