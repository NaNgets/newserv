/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: cleanupdb
 * CMDLEVEL: QCMD_OPER
 * CMDARGS: 0
 * CMDDESC: Clean Up Db
 * CMDFUNC: csu_docleanupdb
 * CMDPROTO: int csu_docleanupdb(void *source, int cargc, char **cargv);
 */

#include "../chanserv.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>

int csu_docleanupdb(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *vrup, *srup;
  int i;
  long to_age = 0L;
  struct tm *tmp;
  char buf[200];
  int j = 0;
  to_age = time(NULL) - (80 * 3600 * 24);  

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (vrup=regusernicktable[i]; vrup; vrup=srup) {
      srup=vrup->nextbyname;
      if(!vrup->nicks && vrup->lastauth < to_age && !UHasHelperPriv(vrup) && !UIsCleanupExempt(vrup)) {
        tmp=gmtime(&(vrup->lastauth));
        strftime(buf,15,"%d/%m/%y %H:%M",tmp);

        cs_removeuser(vrup);
        j++;
      }
    }
  }
  
  chanservsendmessage(sender, "Removed %d Accounts (not used for 80 days)", j);
}
