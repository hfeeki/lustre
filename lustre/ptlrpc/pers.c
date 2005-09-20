/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2004 Cluster File Systems, Inc.
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
#ifndef __KERNEL__
#include <errno.h>
#include <signal.h>
#include <liblustre.h>
#endif

#include <linux/obd_support.h>
#include <linux/obd_class.h>
#include <linux/lustre_lib.h>
#include <linux/lustre_ha.h>
#include <linux/lustre_import.h>

#include "ptlrpc_internal.h"

#ifdef __KERNEL__

void ptlrpc_fill_bulk_md (lnet_md_t *md, struct ptlrpc_bulk_desc *desc)
{
        LASSERT (desc->bd_iov_count <= PTLRPC_MAX_BRW_PAGES);
        LASSERT (!(md->options & (LNET_MD_IOVEC | LNET_MD_KIOV | LNET_MD_PHYS)));

        md->options |= LNET_MD_KIOV;
        md->start = &desc->bd_iov[0];
        md->length = desc->bd_iov_count;
}

void ptlrpc_add_bulk_page(struct ptlrpc_bulk_desc *desc, struct page *page,
                          int pageoffset, int len)
{
        lnet_kiov_t *kiov = &desc->bd_iov[desc->bd_iov_count];

        kiov->kiov_page = page;
        kiov->kiov_offset = pageoffset;
        kiov->kiov_len = len;

        desc->bd_iov_count++;
}

#else /* !__KERNEL__ */

void ptlrpc_fill_bulk_md(lnet_md_t *md, struct ptlrpc_bulk_desc *desc)
{
        LASSERT (!(md->options & (LNET_MD_IOVEC | LNET_MD_KIOV | LNET_MD_PHYS)));
        if (desc->bd_iov_count == 1) {
                md->start = desc->bd_iov[0].iov_base;
                md->length = desc->bd_iov[0].iov_len;
                return;
        }
        
        md->options |= LNET_MD_IOVEC;
        md->start = &desc->bd_iov[0];
        md->length = desc->bd_iov_count;
}

static int can_merge_iovs(lnet_md_iovec_t *existing, lnet_md_iovec_t *candidate)
{
        if (existing->iov_base + existing->iov_len == candidate->iov_base) 
                return 1;
#if 0
        /* Enable this section to provide earlier evidence of fragmented bulk */
        CERROR("Can't merge iovs %p for %x, %p for %x\n",
               existing->iov_base, existing->iov_len,
               candidate->iov_base, candidate->iov_len);
#endif
        return 0;
}

void ptlrpc_add_bulk_page(struct ptlrpc_bulk_desc *desc, struct page *page, 
                          int pageoffset, int len)
{
        lnet_md_iovec_t *iov = &desc->bd_iov[desc->bd_iov_count];

        iov->iov_base = page->addr + pageoffset;
        iov->iov_len = len;

        if (desc->bd_iov_count > 0 && can_merge_iovs(iov - 1, iov)) {
                (iov - 1)->iov_len += len;
        } else {
                desc->bd_iov_count++;
        }
}

#endif /* !__KERNEL__ */
