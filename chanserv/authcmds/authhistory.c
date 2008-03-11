/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: authhistory
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 1
 * CMDDESC: View auth history for an account.
 * CMDFUNC: csa_doauthhistory
 * CMDPROTO: int csa_doauthhistory(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: AUTHHISTORY
 * CMDHELP: Displays details of the last 10 logins with your account.  Details include
 * CMDHELP: hostmask, login time, disconnect time and reason.
 */

#include "../chanserv.h"
#include "../authlib.h"
#include "../../lib/irc_string.h"
#include "../../pqsql/pqsql.h"

#include <stdio.h>
#include <string.h>
#include <libpq-fe.h>

struct authhistoryinfo {
  unsigned int numeric;
  unsigned int userID;
};

void csdb_doauthhistory_real(PGconn *dbconn, void *arg) {
  struct authhistoryinfo *ahi=(struct authhistoryinfo*)arg;
  nick *np=getnickbynumeric(ahi->numeric);
  reguser *rup;
  char *ahnick, *ahuser, *ahhost;
  time_t ahauthtime, ahdisconnecttime;
  PGresult *pgres;
  int i, num, count=0;
  struct tm *tmp;
  char tbuf1[15], tbuf2[15], uhbuf[51];

  if(!dbconn) {
    free(ahi);
    return;
  }

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading auth history data.");
    free(ahi);
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Auth history data format error.");
    PQclear(pgres);
    free(ahi);
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    free(ahi);
    return;
  }

  if (!(rup=getreguserfromnick(np))) {
    PQclear(pgres);
    free(ahi);
    return;
  }
  chanservstdmessage(np, QM_AUTHHISTORYHEADER);
  for (i=0; i<num; i++) {
    if (!UHasHelperPriv(rup) && (strtoul(PQgetvalue(pgres, i, 0), NULL, 10) != rup->ID)) {
      PQclear(pgres);
      free(ahi);
      return;
    }
    ahnick=PQgetvalue(pgres, i, 1);
    ahuser=PQgetvalue(pgres, i, 2);
    ahhost=PQgetvalue(pgres, i, 3);
    ahauthtime=strtoul(PQgetvalue(pgres, i, 4), NULL, 10);
    ahdisconnecttime=strtoul(PQgetvalue(pgres, i, 5), NULL, 10);
    tmp=localtime(&ahauthtime);
    strftime(tbuf1, 15, "%d/%m/%y %H:%M", tmp);
    if (ahdisconnecttime) {
      tmp=localtime(&ahdisconnecttime);
      strftime(tbuf2, 15, "%d/%m/%y %H:%M", tmp);
    }
    snprintf(uhbuf,50,"%s!%s@%s", ahnick, ahuser, ahhost);
    chanservsendmessage(np, "#%-2d %-50s %-15s %-15s %s", ++count, uhbuf, tbuf1, ahdisconnecttime?tbuf2:"never", PQgetvalue(pgres,i,6));
  }
  chanservstdmessage(np, QM_ENDOFLIST);

  PQclear(pgres);
  free(ahi);
}

void csdb_retreiveauthhistory(nick *np, reguser *rup, int limit) {
  struct authhistoryinfo *ahi;

  ahi=(struct authhistoryinfo *)malloc(sizeof(struct authhistoryinfo));
  ahi->numeric=np->numeric;
  ahi->userID=rup->ID;
  q9a_asyncquery(csdb_doauthhistory_real, (void *)ahi,
    "SELECT userID, nick, username, host, authtime, disconnecttime, quitreason from authhistory where "
    "userID=%u order by authtime desc limit %d", rup->ID, limit);
}

int csa_doauthhistory(void *source, int cargc, char **cargv) {
  reguser *rup, *trup;
  nick *sender=source;

  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;

  if (cargc >= 1) {
    if (!(trup=findreguser(sender, cargv[0])))
      return CMD_ERROR;

    /* don't allow non-opers to view oper auth history, but allow helpers to view non-oper history */
    if ((trup != rup) && ((UHasOperPriv(trup) && !UHasOperPriv(rup)) || !UHasHelperPriv(rup))) {
      chanservstdmessage(sender, QM_NOACCESSONUSER, "authhistory", cargv[0]);
      return CMD_ERROR;
    }
  } else {
    trup=rup;
  }
  
  csdb_retreiveauthhistory(sender, trup, 10);

  return CMD_OK;
}
