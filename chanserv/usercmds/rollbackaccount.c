/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: rollbackaccount
 * CMDLEVEL: QCMD_OPER
 * CMDARGS: 2
 * CMDDESC: Roll back password/email changes on an account.
 * CMDFUNC: csa_dorollbackaccount
 * CMDPROTO: int csa_dorollbackaccount(void *source, int cargc, char **cargv);
 */

#include "../chanserv.h"
#include "../../lib/irc_string.h"
#include "../../pqsql/pqsql.h"

#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>

void csdb_dorollbackaccount_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  reguser *rup;
  unsigned int userID;
  char *oldpass, *newpass, *oldemail, *newemail;
  time_t changetime, authtime;
  PGresult *pgres;
  int i, num;

  if(!dbconn)
    return;

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading account rollback data.");
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Account rollback data format error.");
    PQclear(pgres);
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  if (!(rup=getreguserfromnick(np)) || !UHasOperPriv(rup)) {
    Error("chanserv", ERR_ERROR, "No reguser pointer or oper privs in rollback account.");
    PQclear(pgres);
    return;
  }

  chanservsendmessage(np, "Attempting to rollback account %s:", rup->username);
  for (i=0; i<num; i++) {
    userID=strtoul(PQgetvalue(pgres, i, 0), NULL, 10);
    changetime=strtoul(PQgetvalue(pgres, i, 1), NULL, 10);
    authtime=strtoul(PQgetvalue(pgres, i, 2), NULL, 10);
    oldpass=PQgetvalue(pgres, i, 3);
    newpass=PQgetvalue(pgres, i, 4);
    oldemail=PQgetvalue(pgres, i, 5);
    newemail=PQgetvalue(pgres, i, 6);
    if (strlen(newpass) > 0) {
      setpassword(rup, oldpass);
      chanservsendmessage(np, "Restoring old password (%s -> %s)", newpass, oldpass);
    }
    else if (strlen(newemail) > 0) {
      /* WARNING: lastemail untouched */
      freesstring(rup->email);
      rup->email=getsstring(oldemail, EMAILLEN);
      rup->lastemailchange=changetime;
      chanservsendmessage(np, "Restoring old email (%s -> %s)", newemail, oldemail);
    }
  }
  csdb_updateuser(rup);
  chanservstdmessage(np, QM_DONE);

  PQclear(pgres);
}

void csdb_rollbackaccounthistory(nick *np, reguser* rup, time_t starttime) {
  q9u_asyncquery(csdb_dorollbackaccount_real, (void *)np->numeric,
    "SELECT userID, changetime, authtime, oldpassword, newpassword, oldemail, newemail from accounthistory where "
    "userID=%u and changetime>%lu order by changetime desc limit 10", rup->ID, starttime);
}

int csa_dorollbackaccount(void *source, int cargc, char **cargv) {
  reguser *rup, *trup;
  nick *sender=source;
  time_t starttime=getnettime();
  long duration;
  
  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;
  
  if (cargc < 2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "rollbackaccount");
    return CMD_ERROR;
  }
  
  if (!(trup=findreguser(sender, cargv[0])))
    return CMD_ERROR;
  
  if (UHasOperPriv(trup)) {
    chanservstdmessage(sender, QM_NOACCESS, "rollbackaccount", cargv[0]);
    return CMD_ERROR;
  }
  
  if (!(duration=durationtolong(cargv[1]))) {
    chanservsendmessage(sender, "Invalid duration.");
    return CMD_ERROR;
  }
  starttime-=duration;
  
  cs_log(sender,"ROLLBACKACCOUNT %s #%s %s", trup->username, rup->username, cargv[1]);
  
  csdb_rollbackaccounthistory(sender, trup, starttime);
  
  return CMD_OK;
}
