/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Portal-RPC reconnection and replay operations, for use in recovery.
 *
 *  Copyright (c) 2002, 2003 Cluster File Systems, Inc.
 *   Author: Mike Shaver <shaver@clusterfs.com>
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 *   You may have signed or agreed to another license before downloading
 *   this software.  If so, you are bound by the terms and conditions
 *   of that agreement, and the following does not apply to you.  See the
 *   LICENSE file included with this distribution for more information.
 *
 *   If you did not agree to a different license, then this copy of Lustre
 *   is open source software; you can redistribute it and/or modify it
 *   under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   In either case, Lustre is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   license text for more details.
 */

#define DEBUG_SUBSYSTEM S_RPC
#ifdef __KERNEL__
# include <libcfs/libcfs.h>
#else
# include <liblustre.h>
#endif

#include <obd_support.h>
#include <lustre_ha.h>
#include <lustre_net.h>
#include <lustre_import.h>
#include <lustre_export.h>
#include <obd.h>
#include <obd_ost.h>
#include <obd_class.h>
#include <obd_lov.h> /* for IOC_LOV_SET_OSC_ACTIVE */
#include <libcfs/list.h>

#include "ptlrpc_internal.h"

static int ptlrpc_recover_import_no_retry(struct obd_import *, char *);

void ptlrpc_initiate_recovery(struct obd_import *imp)
{
        ENTRY;

        CDEBUG(D_HA, "%s: starting recovery\n", obd2cli_tgt(imp->imp_obd));
        ptlrpc_connect_import(imp, NULL);

        EXIT;
}

int ptlrpc_replay_next(struct obd_import *imp, int *inflight)
{
        int rc = 0;
        struct list_head *tmp, *pos;
        struct ptlrpc_request *req = NULL;
        __u64 last_transno;
        ENTRY;

        *inflight = 0;

        /* It might have committed some after we last spoke, so make sure we
         * get rid of them now.
         */
        spin_lock(&imp->imp_lock);
        imp->imp_last_transno_checked = 0;
        ptlrpc_free_committed(imp);
        last_transno = imp->imp_last_replay_transno;
        spin_unlock(&imp->imp_lock);

        CDEBUG(D_HA, "import %p from %s committed "LPU64" last "LPU64"\n",
               imp, obd2cli_tgt(imp->imp_obd),
               imp->imp_peer_committed_transno, last_transno);

        /* Do I need to hold a lock across this iteration?  We shouldn't be
         * racing with any additions to the list, because we're in recovery
         * and are therefore not processing additional requests to add.  Calls
         * to ptlrpc_free_committed might commit requests, but nothing "newer"
         * than the one we're replaying (it can't be committed until it's
         * replayed, and we're doing that here).  l_f_e_safe protects against
         * problems with the current request being committed, in the unlikely
         * event of that race.  So, in conclusion, I think that it's safe to
         * perform this list-walk without the imp_lock held.
         *
         * But, the {mdc,osc}_replay_open callbacks both iterate
         * request lists, and have comments saying they assume the
         * imp_lock is being held by ptlrpc_replay, but it's not. it's
         * just a little race...
         */
        list_for_each_safe(tmp, pos, &imp->imp_replay_list) {
                req = list_entry(tmp, struct ptlrpc_request, rq_replay_list);

                /* If need to resend the last sent transno (because a
                   reconnect has occurred), then stop on the matching
                   req and send it again. If, however, the last sent
                   transno has been committed then we continue replay
                   from the next request. */
                if (imp->imp_resend_replay && 
                    req->rq_transno == last_transno) {
                        lustre_msg_add_flags(req->rq_reqmsg, MSG_RESENT);
                        break;
                }

                if (req->rq_transno > last_transno) {
                        imp->imp_last_replay_transno = req->rq_transno;
                        break;
                }

                req = NULL;
        }

        imp->imp_resend_replay = 0;

        if (req != NULL) {
                rc = ptlrpc_replay_req(req);
                if (rc) {
                        CERROR("recovery replay error %d for req "
                               LPD64"\n", rc, req->rq_xid);
                        RETURN(rc);
                }
                *inflight = 1;
        }
        RETURN(rc);
}

int ptlrpc_resend(struct obd_import *imp)
{
        struct ptlrpc_request *req, *next;

        ENTRY;

        /* As long as we're in recovery, nothing should be added to the sending
         * list, so we don't need to hold the lock during this iteration and
         * resend process.
         */
        /* Well... what if lctl recover is called twice at the same time?
         */
        spin_lock(&imp->imp_lock);
        if (imp->imp_state != LUSTRE_IMP_RECOVER) {
                spin_unlock(&imp->imp_lock);
                RETURN(-1);
        }
        spin_unlock(&imp->imp_lock);

        list_for_each_entry_safe(req, next, &imp->imp_sending_list, rq_list) {
                LASSERTF((long)req > CFS_PAGE_SIZE && req != LP_POISON,
                         "req %p bad\n", req);
                LASSERTF(req->rq_type != LI_POISON, "req %p freed\n", req);
                if (!req->rq_no_resend)
                        ptlrpc_resend_req(req);
        }

        RETURN(0);
}

void ptlrpc_wake_delayed(struct obd_import *imp)
{
        struct list_head *tmp, *pos;
        struct ptlrpc_request *req;

        spin_lock(&imp->imp_lock);
        list_for_each_safe(tmp, pos, &imp->imp_delayed_list) {
                req = list_entry(tmp, struct ptlrpc_request, rq_list);

                DEBUG_REQ(D_HA, req, "waking (set %p):", req->rq_set);
                ptlrpc_wake_client_req(req);
        }
        spin_unlock(&imp->imp_lock);
}

void ptlrpc_request_handle_notconn(struct ptlrpc_request *failed_req)
{
        struct obd_import *imp = failed_req->rq_import;
        ENTRY;

        CDEBUG(D_HA, "import %s of %s@%s abruptly disconnected: reconnecting\n",
               imp->imp_obd->obd_name, obd2cli_tgt(imp->imp_obd),
               imp->imp_connection->c_remote_uuid.uuid);

        if (ptlrpc_set_import_discon(imp,
                              lustre_msg_get_conn_cnt(failed_req->rq_reqmsg))) {
                if (!imp->imp_replayable) {
                        CDEBUG(D_HA, "import %s@%s for %s not replayable, "
                               "auto-deactivating\n",
                               obd2cli_tgt(imp->imp_obd),
                               imp->imp_connection->c_remote_uuid.uuid,
                               imp->imp_obd->obd_name);
                        ptlrpc_deactivate_import(imp);
                }
                /* to control recovery via lctl {disable|enable}_recovery */
                if (imp->imp_deactive == 0)
                        ptlrpc_connect_import(imp, NULL);
        }

        /* Wait for recovery to complete and resend. If evicted, then
           this request will be errored out later.*/
        spin_lock(&failed_req->rq_lock);
        if (!failed_req->rq_no_resend)
                failed_req->rq_resend = 1;
        spin_unlock(&failed_req->rq_lock);

        EXIT;
}

/*
 * Administratively active/deactive a client. 
 * This should only be called by the ioctl interface, currently
 *  - the lctl deactivate and activate commands
 *  - echo 0/1 >> /proc/osc/XXX/active
 *  - client umount -f (ll_umount_begin)
 */
int ptlrpc_set_import_active(struct obd_import *imp, int active)
{
        struct obd_device *obd = imp->imp_obd;
        int rc = 0;

        ENTRY;
        LASSERT(obd);

        /* When deactivating, mark import invalid, and abort in-flight
         * requests. */
        if (!active) {
                LCONSOLE_WARN("setting import %s INACTIVE by administrator "
                              "request\n", obd2cli_tgt(imp->imp_obd));
                ptlrpc_invalidate_import(imp);
                imp->imp_deactive = 1;
        }

        /* When activating, mark import valid, and attempt recovery */
        if (active) {
                imp->imp_deactive = 0;
                CDEBUG(D_HA, "setting import %s VALID\n",
                       obd2cli_tgt(imp->imp_obd));
                rc = ptlrpc_recover_import(imp, NULL);
        }

        RETURN(rc);
}

/* Attempt to reconnect an import */
int ptlrpc_recover_import(struct obd_import *imp, char *new_uuid)
{
        int rc;
        ENTRY;

        /* force import to be disconnected. */
        ptlrpc_set_import_discon(imp, 0);

        imp->imp_deactive = 0;
        rc = ptlrpc_recover_import_no_retry(imp, new_uuid);

        RETURN(rc);
}

int ptlrpc_import_in_recovery(struct obd_import *imp)
{
        int in_recovery = 1;
        spin_lock(&imp->imp_lock);
        if (imp->imp_state == LUSTRE_IMP_FULL ||
            imp->imp_state == LUSTRE_IMP_CLOSED ||
            imp->imp_state == LUSTRE_IMP_DISCON)
                in_recovery = 0;
        spin_unlock(&imp->imp_lock);
        return in_recovery;
}

static int ptlrpc_recover_import_no_retry(struct obd_import *imp,
                                          char *new_uuid)
{
        int rc;
        int in_recovery = 0;
        struct l_wait_info lwi;
        ENTRY;

        /* Check if reconnect is already in progress */
        spin_lock(&imp->imp_lock);
        if (imp->imp_state != LUSTRE_IMP_DISCON) {
                in_recovery = 1;
        }
        spin_unlock(&imp->imp_lock);

        if (in_recovery == 1)
                RETURN(-EALREADY);

        rc = ptlrpc_connect_import(imp, new_uuid);
        if (rc)
                RETURN(rc);

        CDEBUG(D_HA, "%s: recovery started, waiting\n",
               obd2cli_tgt(imp->imp_obd));

        lwi = LWI_TIMEOUT(cfs_timeout_cap(cfs_time_seconds(obd_timeout)), 
                          NULL, NULL);
        rc = l_wait_event(imp->imp_recovery_waitq,
                          !ptlrpc_import_in_recovery(imp), &lwi);
        CDEBUG(D_HA, "%s: recovery finished\n",
               obd2cli_tgt(imp->imp_obd));

        RETURN(rc);
}
