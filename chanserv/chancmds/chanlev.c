/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: chanlev
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 3
 * CMDDESC: Shows or modifies user access on a channel.
 * CMDFUNC: csc_dochanlev
 * CMDPROTO: int csc_dochanlev(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: CHANLEV <channel> [<user> [<change>]]
 * CMDHELP: Displays or alters the access of known users on a channel, where:
 * CMDHELP: channel - the channel to use
 * CMDHELP: user    - the user to list or modify.  user can be specified as either an active
 * CMDHELP:           nickname on the network or #accountname.  If user is not specified then
 * CMDHELP:           all known users are listed.
 * CMDHELP: change  - lists the flags to add or remove, with + to add or - to remove.  For
 * CMDHELP:           example, +ao to add a and o flags, or -gv to remove g and v flags.  This 
 * CMDHELP:           can be used to add or remove users from the channel.  If change is not
 * CMDHELP:           specified then the current access of the named user is displayed.
 * CMDHELP: Displaying known user information requires you to be known (+k) on the named channel.
 * CMDHELP: Adjusting flags for other users requires master (+m) access on the named channel.
 * CMDHELP: Adding or removing the +m flag for other users requires owner (+n) access on the 
 * CMDHELP:  named channel.
 * CMDHELP: You may always remove your own flags, except +qdb flags (which are not visible to you).
 * CMDHELP: Adding or removing personal flags requires you to be known (+k) on the named channel.
 * CMDHELP: Note that channel owners (+n) can grant +n to channel masters but they must use 
 * CMDHELP: the GIVEOWNER command for this.
 * CMDHELP: The access level flags determine which commands a user is allowed to use on a channel.
 * CMDHELP: Holding an access flag also grants access to any action requiring a lesser flag (e.g.
 * CMDHELP: +m users can perform actions requiring operator (+o) status even if they do not
 * CMDHELP: actually have +o set).  The access flags are listed in descending order.
 * CMDHELP: Valid flags are:
 * CMDHELP: Access level flags - these control the user's overall privilege level on the channel:
 * CMDHELP:  +n OWNER     Can add or remove masters and all other flags (except personal flags)
 * CMDHELP:  +m MASTER    Can add or remove all access except master or owner
 * CMDHELP:  +o OP        Can get ops on the channel
 * CMDHELP:  +v VOICE     Can get voice on the channel
 * CMDHELP:  +k KNOWN     Known on the channel - can get invites to the channel via INVITE
 * CMDHELP: Punishment flags - these restrict the user on the channel in some way:
 * CMDHELP:  +q DEVOICE   Not allowed to be voiced on the channel
 * CMDHELP:  +d DEOP      Not allowed to be opped on the channel
 * CMDHELP:  +b BANNED    Banned from the channel
 * CMDHELP: Extra flags - these control specific behaviour on the channel:
 * CMDHELP:  +a AUTOOP    Ops the user automatically when they join the channel (the user 
 * CMDHELP:               must also hold +o in order to have this flag)
 * CMDHELP:  +g AUTOVOICE Voices the user automatically when they join the channel (the 
 * CMDHELP:               user must also hold +v in order to have this flag)
 * CMDHELP:  +p PROTECT   If the user has +o or +v, this makes sure they will always have
 * CMDHELP:               that status, they will be reopped/voiced if deopped/voiced
 * CMDHELP:  +t TOPIC     Can use SETTOPIC to alter the topic on the channel
 * CMDHELP: Personal flags - these control user personal preferences and can only be changed
 * CMDHELP:                  by the user concerned.  They are not visible to other users.
 * CMDHELP:  +w NOWELCOME Prevents the welcome message being sent when you join the channel.
 * CMDHELP:  +j AUTOINV   Invites you to the channel automatically when you authenticate.
 * CMDHELP: Note that non-sensible combinations of flags are not allowed.  After making a 
 * CMDHELP: change the current status of the named user on the channel will be confirmed.
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

int compareflags(const void *u1, const void *u2) {
  const regchanuser *r1=*(void **)u1, *r2=*(void **)u2;
  flag_t f1,f2;

  for (f1=QCUFLAG_OWNER;f1;f1>>=1)
    if (r1->flags & f1)
      break;

  for (f2=QCUFLAG_OWNER;f2;f2>>=1)
    if (r2->flags & f2)
      break;

  if (f1==f2) {
    return ircd_strcmp(r1->user->username, r2->user->username);
  } else {
    return f2-f1;
  }
}

int csc_dochanlev(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup, *rcuplist;
  regchanuser **rusers;
  reguser *rup=getreguserfromnick(sender), *target;
  char time1[TIMELEN],time2[TIMELEN];
  char flagbuf[30];
  flag_t flagmask, changemask, flags, oldflags;
  int showtimes=0;
  int donehead=0;
  int i,j;
  int newuser=0;
  int usercount;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chanlev");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_KNOWN,
			   NULL, "chanlev", QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];
  rcup=findreguseronchannel(rcp, rup);

  /* Set flagmask for +v/+o users (can't see bans etc.) */
  flagmask = (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOVOICE | 
	      QCUFLAG_AUTOOP | QCUFLAG_TOPIC | QCUFLAG_PROTECT | QCUFLAG_KNOWN);
  
  /* masters and above can see everything except personal flags */
  if (rcup && CUHasMasterPriv(rcup)) {
    flagmask = QCUFLAG_ALL & ~QCUFLAGS_PERSONAL;
    showtimes=1;
  }
  
  /* Staff access, show everything */
  if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender)) {
    flagmask = QCUFLAG_ALL;
    showtimes=1;
  }
  
  if (cargc==1) {
    /* One arg: list chanlev */
    int ncnt=0,mcnt=0,ocnt=0,vcnt=0,kcnt=0,bcnt=0;
    
    if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender)) {
      reguser *founder=NULL, *addedby=NULL;
      addedby=findreguserbyID(rcp->addedby);
      chanservstdmessage(sender, QM_ADDEDBY, addedby ? addedby->username : "(unknown)");
      founder=findreguserbyID(rcp->founder);
      chanservstdmessage(sender, QM_FOUNDER, founder ? founder->username : "(unknown)");
      if (rcp->chantype) {
        chanservstdmessage(sender, QM_CHANTYPE, chantypes[rcp->chantype]->content);
      }
    }

    if (CIsSuspended(rcp) && cs_privcheck(QPRIV_VIEWCHANSUSPENSION, sender)) {
      char *bywhom;

      if(cs_privcheck(QPRIV_VIEWSUSPENDEDBY, sender)) {
        reguser *trup = findreguserbyID(rcp->suspendby);
        if(trup) {
          bywhom = trup->username;
        } else {
         bywhom = "(unknown)";
        }
      } else {
        bywhom = "(hidden)";
      }

      chanservstdmessage(sender, QM_CHANLEV_SUSPENDREASON, rcp->suspendreason?rcp->suspendreason->content:"(no reason)");
      chanservstdmessage(sender, QM_CHANLEV_SUSPENDBY, bywhom);
      chanservstdmessage(sender, QM_CHANLEV_SUSPENDSINCE, rcp->suspendtime);
    }

    if (rcp->comment && (cs_privcheck(QPRIV_VIEWCOMMENTS, sender)))
      chanservstdmessage(sender, QM_SHORT_COMMENT, rcp->comment->content);

    /* Count users */
    for (i=0,usercount=0;i<REGCHANUSERHASHSIZE;i++)
      for (rcuplist=rcp->regusers[i];rcuplist;rcuplist=rcuplist->nextbychan)
	usercount++;
    
    /* Allocate array */
    rusers=(regchanuser **)malloc(usercount * sizeof(regchanuser *));

    /* Fill array */
    for (j=i=0;i<REGCHANUSERHASHSIZE;i++) {
      for (rcuplist=rcp->regusers[i];rcuplist;rcuplist=rcuplist->nextbychan) {
	if (!(flags=rcuplist->flags & flagmask))
	  continue;
	
	rusers[j++]=rcuplist;
      }
    }

    /* Sort */
    qsort(rusers, j, sizeof(regchanuser *), compareflags);

    /* List */
    for (i=0;i<j;i++) {
      rcuplist=rusers[i];

      if (!(flags=rcuplist->flags & flagmask)) 
	continue;
      
      /* If you're listing yourself, we should show personal flags too */
      if (rcuplist==rcup) {
        flags=rcuplist->flags & (flagmask | QCUFLAGS_PERSONAL);
      }
      
      /* Do the count here; note that +n's aren't counted as +m (and so on).  We're not
       * using the IsX() macros because the displayed count needs to match up with 
       * the displayed flags... */
      if (flags & QCUFLAG_OWNER) ncnt++; else
      if (flags & QCUFLAG_MASTER) mcnt++; else
      if (flags & QCUFLAG_OP) ocnt++; else
      if (flags & QCUFLAG_VOICE) vcnt++; else
      if (flags & QCUFLAG_KNOWN) kcnt++;
      if (flags & QCUFLAG_BANNED) bcnt++;
      
      if (!donehead) {
	chanservstdmessage(sender, QM_CHANLEVHEADER, cip->name->content);
	if (showtimes) 
	  chanservstdmessage(sender, QM_CHANLEVCOLFULL);
	else
	  chanservstdmessage(sender, QM_CHANLEVCOLSHORT);
	donehead=1;
      }
      
      if (showtimes) {
	if (!rcuplist->usetime) {
	  strcpy(time1,"Never");
	} else {
	  q9strftime(time1,sizeof(time1),rcuplist->usetime);
	}
	if (!rcuplist->changetime) {
	  strcpy(time2, "Unknown");
	} else {
	  q9strftime(time2,sizeof(time2),rcuplist->changetime);
	}
	chanservsendmessage(sender, " %-15s %-13s %-14s  %-14s  %s", rcuplist->user->username, 
			    printflags(flags, rcuflags), time1, time2, rcuplist->info?rcuplist->info->content:"");
      } else 
	chanservsendmessage(sender, " %-15s %s", rcuplist->user->username, printflags(flags, rcuflags));
    }
    
    if (donehead) {
      chanservstdmessage(sender, QM_ENDOFLIST);
      chanservstdmessage(sender, QM_CHANLEVSUMMARY, j, ncnt, mcnt, ocnt, vcnt, kcnt, bcnt);
    } else {
      chanservstdmessage(sender, QM_NOUSERSONCHANLEV, cip->name->content);
    }

    free(rusers);
  } else {
    /* 2 or more args.. relates to one specific user */
    if (!(target=findreguser(sender, cargv[1])))
      return CMD_ERROR; /* If there was an error, findreguser will have sent a message saying why.. */
    
    rcuplist=findreguseronchannel(rcp, target);
    
    if (cargc>2) {
      /* To change chanlev you have to either.. */
      if (!( cs_privcheck(QPRIV_CHANGECHANLEV, sender) ||             /* Have override privilege */
	     (rcup && rcuplist && (rcup==rcuplist) && CUKnown(rcup)) || /* Be manipulting yourself (oo er..) */
	     (rcup && CUHasMasterPriv(rcup) &&                        /* Have +m or +n on the channel */
	      !(rcuplist && CUIsOwner(rcuplist) && !CUIsOwner(rcup))) /* masters can't screw with owners */
	     )) {
	chanservstdmessage(sender, QM_NOACCESSONCHAN, cip->name->content, "chanlev");
	return CMD_ERROR;
      }
      
      if (!rcuplist) {
        /* new user, we could store a count instead... that's probably better... */
        unsigned int count;

        for (count=i=0;i<REGCHANUSERHASHSIZE;i++)
          for (rcuplist=rcp->regusers[i];rcuplist;rcuplist=rcuplist->nextbychan)
            count++;

        if(count >= MAXCHANLEVS) {
          chanservstdmessage(sender, QM_TOOMANYCHANLEVS);
          return CMD_ERROR;
        }
   
	rcuplist=getregchanuser();
	rcuplist->user=target;
	rcuplist->chan=rcp;
	rcuplist->flags=0;
	rcuplist->changetime=time(NULL);
	rcuplist->usetime=0;
	rcuplist->info=NULL;
	newuser=1;
      }
      
      if (cs_privcheck(QPRIV_CHANGECHANLEV, sender)) {
	/* Opers are allowed to change everything */
	changemask = QCUFLAG_ALL;
      } else {
	changemask=0;
	
	/* Everyone can change their own flags (except +dqb), and control (and see) personal flags */
	if (rcup==rcuplist) {
	  changemask = (rcup->flags | QCUFLAGS_PERSONAL) & ~(QCUFLAGS_PUNISH);
	  flagmask |= QCUFLAGS_PERSONAL;
	}
	
	/* Masters are allowed to manipulate +ovagtbqdpk */
	if (CUHasMasterPriv(rcup))
	  changemask |= ( QCUFLAG_KNOWN | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOOP | QCUFLAG_AUTOVOICE | 
			  QCUFLAG_TOPIC | QCUFLAG_PROTECT | QCUFLAGS_PUNISH);
	
	/* Owners are allowed to manipulate +ms as well.
	 * We allow +n to be given initially, but we check later to see if the flag has been added.
	 * if it has, abort and say "use giveowner"
	 */
	if (CUIsOwner(rcup))
	  changemask |= ( QCUFLAG_MASTER | QCUFLAG_OWNER );
      }

      oldflags=rcuplist->flags;
      if (setflags(&(rcuplist->flags), changemask, cargv[2], rcuflags, REJECT_UNKNOWN | REJECT_DISALLOWED)) {
	chanservstdmessage(sender, QM_INVALIDCHANLEVCHANGE);
        if (newuser)
          freeregchanuser(rcuplist);
	return CMD_ERROR;
      }

      /* check to see if +n has been given.  Opers can bypass this. */
      if (!cs_privcheck(QPRIV_CHANGECHANLEV, sender) && !(oldflags & QCUFLAG_OWNER) && (rcuplist->flags & QCUFLAG_OWNER)) {
        rcuplist->flags=oldflags;
	chanservstdmessage(sender, QM_USEGIVEOWNER);
        if (newuser)
          freeregchanuser(rcuplist);
	return CMD_ERROR;
      }

      /* Fix up impossible combinations */
      rcuplist->flags = cs_sanitisechanlev(rcuplist->flags);

      /* Check if anything "significant" has changed */
      if ((oldflags ^ rcuplist->flags) & (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP))
	rcuplist->changetime=time(NULL);

      if(rcuplist->flags == oldflags) {
        chanservstdmessage(sender, QM_CHANLEVNOCHANGE);
        if (newuser)
          freeregchanuser(rcuplist);
        return CMD_OK;
      }

      strcpy(flagbuf,printflags(oldflags,rcuflags));
      cs_log(sender,"CHANLEV %s #%s %s (%s -> %s)",cip->name->content,rcuplist->user->username,cargv[2],
	     flagbuf,printflags(rcuplist->flags,rcuflags));
      csdb_chanlevhistory_insert(rcp, sender, rcuplist->user, oldflags, rcuplist->flags);

      /* Now see what we do next */
      if (rcuplist->flags) {
	/* User still valid: update or create */
	if (newuser) {
	  addregusertochannel(rcuplist);
	  csdb_createchanuser(rcuplist);
	} else {
	  csdb_updatechanuser(rcuplist);
	}
	chanservstdmessage(sender, QM_CHANLEVCHANGED, cargv[1], cip->name->content, 
                           printflags(rcuplist->flags & flagmask, rcuflags));
      } else {
	/* User has no flags: delete */
	if (!newuser) {
	  chanservstdmessage(sender, QM_CHANLEVREMOVED, cargv[1], cip->name->content);
	  csdb_deletechanuser(rcuplist);
	  delreguserfromchannel(rcp, target);
	}
	freeregchanuser(rcuplist);
	rcuplist=NULL;
	if (cs_removechannelifempty(sender, rcp)) {
	  chanservstdmessage(sender, QM_CHANLEVEMPTIEDCHANNEL);
          return CMD_OK;
        }
      }
      
      /* Update the channel if needed */
      rcp->status |= QCSTAT_OPCHECK;
      cs_timerfunc(cip);
    } else {
      if (rcuplist == rcup)
        flagmask |= QCUFLAGS_PERSONAL;
      if (rcuplist && (rcuplist->flags & flagmask)) {
        chanservstdmessage(sender, QM_CHANUSERFLAGS, cargv[1], cip->name->content, 
                           printflags(rcuplist->flags & flagmask, rcuflags));
      } else {
        chanservstdmessage(sender, QM_CHANUSERUNKNOWN, cargv[1], cip->name->content);
      }
    }
  }
  
  return CMD_OK;
}
