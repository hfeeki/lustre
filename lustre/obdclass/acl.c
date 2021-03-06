/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright (c) 2012, Intel Corporation.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lustre/obdclass/acl.c
 *
 * Lustre Access Control List.
 *
 * Author: Fan Yong <fanyong@clusterfs.com>
 */

#define DEBUG_SUBSYSTEM S_SEC

#include <lustre_acl.h>
#include <lustre_eacl.h>
#include <obd_support.h>

#ifdef CONFIG_FS_POSIX_ACL

#define CFS_ACL_XATTR_VERSION POSIX_ACL_XATTR_VERSION

enum {
        ES_UNK  = 0,    /* unknown stat */
        ES_UNC  = 1,    /* ACL entry is not changed */
        ES_MOD  = 2,    /* ACL entry is modified */
        ES_ADD  = 3,    /* ACL entry is added */
        ES_DEL  = 4     /* ACL entry is deleted */
};

static inline void lustre_ext_acl_le_to_cpu(ext_acl_xattr_entry *d,
                                            ext_acl_xattr_entry *s)
{
        d->e_tag        = le16_to_cpu(s->e_tag);
        d->e_perm       = le16_to_cpu(s->e_perm);
        d->e_id         = le32_to_cpu(s->e_id);
        d->e_stat       = le32_to_cpu(s->e_stat);
}

static inline void lustre_ext_acl_cpu_to_le(ext_acl_xattr_entry *d,
                                            ext_acl_xattr_entry *s)
{
        d->e_tag        = cpu_to_le16(s->e_tag);
        d->e_perm       = cpu_to_le16(s->e_perm);
        d->e_id         = cpu_to_le32(s->e_id);
        d->e_stat       = cpu_to_le32(s->e_stat);
}

static inline void lustre_posix_acl_le_to_cpu(posix_acl_xattr_entry *d,
                                              posix_acl_xattr_entry *s)
{
        d->e_tag        = le16_to_cpu(s->e_tag);
        d->e_perm       = le16_to_cpu(s->e_perm);
        d->e_id         = le32_to_cpu(s->e_id);
}

static inline void lustre_posix_acl_cpu_to_le(posix_acl_xattr_entry *d,
                                              posix_acl_xattr_entry *s)
{
        d->e_tag        = cpu_to_le16(s->e_tag);
        d->e_perm       = cpu_to_le16(s->e_perm);
        d->e_id         = cpu_to_le32(s->e_id);
}

/*
 * Check permission based on POSIX ACL.
 */
int lustre_posix_acl_permission(struct lu_ucred *mu, struct lu_attr *la,
				int want, posix_acl_xattr_entry *entry,
				int count)
{
        posix_acl_xattr_entry *pa, *pe, *mask_obj;
        posix_acl_xattr_entry ae, me;
        int found = 0;

        if (count <= 0)
                return -EACCES;

        for (pa = &entry[0], pe = &entry[count - 1]; pa <= pe; pa++) {
                lustre_posix_acl_le_to_cpu(&ae, pa);
                switch (ae.e_tag) {
                case ACL_USER_OBJ:
                        /* (May have been checked already) */
			if (la->la_uid == mu->uc_fsuid)
				goto check_perm;
                        break;
                case ACL_USER:
			if (ae.e_id == mu->uc_fsuid)
				goto mask;
                        break;
                case ACL_GROUP_OBJ:
                        if (lustre_in_group_p(mu, la->la_gid)) {
                                found = 1;
                                if ((ae.e_perm & want) == want)
                                        goto mask;
                        }
                        break;
                case ACL_GROUP:
                        if (lustre_in_group_p(mu, ae.e_id)) {
                                found = 1;
                                if ((ae.e_perm & want) == want)
                                        goto mask;
                        }
                        break;
                case ACL_MASK:
                        break;
                case ACL_OTHER:
                        if (found)
                                return -EACCES;
                        else
                                goto check_perm;
                default:
                        return -EIO;
                }
        }
        return -EIO;

mask:
        for (mask_obj = pa + 1; mask_obj <= pe; mask_obj++) {
                lustre_posix_acl_le_to_cpu(&me, mask_obj);
                if (me.e_tag == ACL_MASK) {
                        if ((ae.e_perm & me.e_perm & want) == want)
                                return 0;

                        return -EACCES;
                }
        }

check_perm:
        if ((ae.e_perm & want) == want)
                return 0;

        return -EACCES;
}
EXPORT_SYMBOL(lustre_posix_acl_permission);

/*
 * Modify the ACL for the chmod.
 */
int lustre_posix_acl_chmod_masq(posix_acl_xattr_entry *entry, __u32 mode,
                                int count)
{
	posix_acl_xattr_entry *group_obj = NULL, *mask_obj = NULL, *pa, *pe;

        for (pa = &entry[0], pe = &entry[count - 1]; pa <= pe; pa++) {
		switch (le16_to_cpu(pa->e_tag)) {
		case ACL_USER_OBJ:
			pa->e_perm = cpu_to_le16((mode & S_IRWXU) >> 6);
			break;
		case ACL_USER:
		case ACL_GROUP:
			break;
		case ACL_GROUP_OBJ:
			group_obj = pa;
			break;
		case ACL_MASK:
			mask_obj = pa;
			break;
		case ACL_OTHER:
			pa->e_perm = cpu_to_le16(mode & S_IRWXO);
			break;
		default:
			return -EIO;
		}
	}

	if (mask_obj) {
		mask_obj->e_perm = cpu_to_le16((mode & S_IRWXG) >> 3);
	} else {
		if (!group_obj)
			return -EIO;
		group_obj->e_perm = cpu_to_le16((mode & S_IRWXG) >> 3);
	}

	return 0;
}
EXPORT_SYMBOL(lustre_posix_acl_chmod_masq);

/*
 * Returns 0 if the acl can be exactly represented in the traditional
 * file mode permission bits, or else 1. Returns -E... on error.
 */
	int
lustre_posix_acl_equiv_mode(posix_acl_xattr_entry *entry, mode_t *mode_p,
		int count)
{
	posix_acl_xattr_entry *pa, *pe;
	mode_t                 mode = 0;
	int                    not_equiv = 0;

	for (pa = &entry[0], pe = &entry[count - 1]; pa <= pe; pa++) {
		__u16 perm = le16_to_cpu(pa->e_perm);
		switch (le16_to_cpu(pa->e_tag)) {
			case ACL_USER_OBJ:
				mode |= (perm & S_IRWXO) << 6;
				break;
			case ACL_GROUP_OBJ:
				mode |= (perm & S_IRWXO) << 3;
				break;
			case ACL_OTHER:
				mode |= perm & S_IRWXO;
				break;
			case ACL_MASK:
				mode = (mode & ~S_IRWXG) |
					((perm & S_IRWXO) << 3);
				not_equiv = 1;
				break;
			case ACL_USER:
			case ACL_GROUP:
				not_equiv = 1;
				break;
			default:
				return -EINVAL;
		}
	}
	if (mode_p)
		*mode_p = (*mode_p & ~S_IRWXUGO) | mode;
	return not_equiv;
}
EXPORT_SYMBOL(lustre_posix_acl_equiv_mode);

/*
 * Modify acl when creating a new object.
 */
int lustre_posix_acl_create_masq(posix_acl_xattr_entry *entry, __u32 *pmode,
                                 int count)
{
        posix_acl_xattr_entry *group_obj = NULL, *mask_obj = NULL, *pa, *pe;
        posix_acl_xattr_entry ae;
	__u32 mode = *pmode;
	int not_equiv = 0;

        for (pa = &entry[0], pe = &entry[count - 1]; pa <= pe; pa++) {
                lustre_posix_acl_le_to_cpu(&ae, pa);
                switch (ae.e_tag) {
                case ACL_USER_OBJ:
                        ae.e_perm &= (mode >> 6) | ~S_IRWXO;
			pa->e_perm = cpu_to_le16(ae.e_perm);
			mode &= (ae.e_perm << 6) | ~S_IRWXU;
			break;
		case ACL_USER:
		case ACL_GROUP:
			not_equiv = 1;
			break;
                case ACL_GROUP_OBJ:
			group_obj = pa;
                        break;
                case ACL_OTHER:
                        ae.e_perm &= mode | ~S_IRWXO;
			pa->e_perm = cpu_to_le16(ae.e_perm);
			mode &= ae.e_perm | ~S_IRWXO;
                        break;
                case ACL_MASK:
			mask_obj = pa;
			not_equiv = 1;
                        break;
		default:
			return -EIO;
                }
        }

	if (mask_obj) {
		ae.e_perm = le16_to_cpu(mask_obj->e_perm) &
                            ((mode >> 3) | ~S_IRWXO);
		mode &= (ae.e_perm << 3) | ~S_IRWXG;
                mask_obj->e_perm = cpu_to_le16(ae.e_perm);
	} else {
		if (!group_obj)
			return -EIO;
		ae.e_perm = le16_to_cpu(group_obj->e_perm) &
                            ((mode >> 3) | ~S_IRWXO);
		mode &= (ae.e_perm << 3) | ~S_IRWXG;
                group_obj->e_perm = cpu_to_le16(ae.e_perm);
	}

	*pmode = (*pmode & ~S_IRWXUGO) | mode;
        return not_equiv;
}
EXPORT_SYMBOL(lustre_posix_acl_create_masq);

/* if "new_count == 0", then "new = {a_version, NULL}", NOT NULL. */
static int lustre_posix_acl_xattr_reduce_space(posix_acl_xattr_header **header,
                                               int old_count, int new_count)
{
        int old_size = CFS_ACL_XATTR_SIZE(old_count, posix_acl_xattr);
        int new_size = CFS_ACL_XATTR_SIZE(new_count, posix_acl_xattr);
        posix_acl_xattr_header *new;

        if (unlikely(old_count <= new_count))
                return old_size;

        OBD_ALLOC(new, new_size);
        if (unlikely(new == NULL))
                return -ENOMEM;

        memcpy(new, *header, new_size);
        OBD_FREE(*header, old_size);
        *header = new;
        return new_size;
}

/* if "new_count == 0", then "new = {0, NULL}", NOT NULL. */
static int lustre_ext_acl_xattr_reduce_space(ext_acl_xattr_header **header,
                                             int old_count)
{
        int ext_count = le32_to_cpu((*header)->a_count);
        int ext_size = CFS_ACL_XATTR_SIZE(ext_count, ext_acl_xattr);
        int old_size = CFS_ACL_XATTR_SIZE(old_count, ext_acl_xattr);
        ext_acl_xattr_header *new;

        if (unlikely(old_count <= ext_count))
                return 0;

        OBD_ALLOC(new, ext_size);
        if (unlikely(new == NULL))
                return -ENOMEM;

        memcpy(new, *header, ext_size);
        OBD_FREE(*header, old_size);
        *header = new;
        return 0;
}

/*
 * Generate new extended ACL based on the posix ACL.
 */
ext_acl_xattr_header *
lustre_posix_acl_xattr_2ext(posix_acl_xattr_header *header, int size)
{
        int count, i, esize;
        ext_acl_xattr_header *new;
        ENTRY;

        if (unlikely(size < 0))
                RETURN(ERR_PTR(-EINVAL));
        else if (!size)
                count = 0;
        else
                count = CFS_ACL_XATTR_COUNT(size, posix_acl_xattr);
        esize = CFS_ACL_XATTR_SIZE(count, ext_acl_xattr);
        OBD_ALLOC(new, esize);
        if (unlikely(new == NULL))
                RETURN(ERR_PTR(-ENOMEM));

        new->a_count = cpu_to_le32(count);
        for (i = 0; i < count; i++) {
                new->a_entries[i].e_tag  = header->a_entries[i].e_tag;
                new->a_entries[i].e_perm = header->a_entries[i].e_perm;
                new->a_entries[i].e_id   = header->a_entries[i].e_id;
                new->a_entries[i].e_stat = cpu_to_le32(ES_UNK);
        }

        RETURN(new);
}
EXPORT_SYMBOL(lustre_posix_acl_xattr_2ext);

/*
 * Filter out the "nobody" entries in the posix ACL.
 */
int lustre_posix_acl_xattr_filter(posix_acl_xattr_header *header, int size,
                                  posix_acl_xattr_header **out)
{
        int count, i, j, rc = 0;
        __u32 id;
        posix_acl_xattr_header *new;
        ENTRY;

        if (unlikely(size < 0))
                RETURN(-EINVAL);
        else if (!size)
                RETURN(0);

        OBD_ALLOC(new, size);
        if (unlikely(new == NULL))
                RETURN(-ENOMEM);

        new->a_version = cpu_to_le32(CFS_ACL_XATTR_VERSION);
        count = CFS_ACL_XATTR_COUNT(size, posix_acl_xattr);
        for (i = 0, j = 0; i < count; i++) {
                id = le32_to_cpu(header->a_entries[i].e_id);
                switch (le16_to_cpu(header->a_entries[i].e_tag)) {
                case ACL_USER_OBJ:
                case ACL_GROUP_OBJ:
                case ACL_MASK:
                case ACL_OTHER:
                        if (id != ACL_UNDEFINED_ID)
                                GOTO(_out, rc = -EIO);

                        memcpy(&new->a_entries[j++], &header->a_entries[i],
                               sizeof(posix_acl_xattr_entry));
                        break;
                case ACL_USER:
                        if (id != NOBODY_UID)
                                memcpy(&new->a_entries[j++],
                                       &header->a_entries[i],
                                       sizeof(posix_acl_xattr_entry));
                        break;
                case ACL_GROUP:
                        if (id != NOBODY_GID)
                                memcpy(&new->a_entries[j++],
                                       &header->a_entries[i],
                                       sizeof(posix_acl_xattr_entry));
                        break;
                default:
                        GOTO(_out, rc = -EIO);
                }
        }

        /* free unused space. */
        rc = lustre_posix_acl_xattr_reduce_space(&new, count, j);
        if (rc >= 0) {
                size = rc;
                *out = new;
                rc = 0;
        }
        EXIT;

_out:
        if (rc) {
                OBD_FREE(new, size);
                size = rc;
        }
        return size;
}
EXPORT_SYMBOL(lustre_posix_acl_xattr_filter);

/*
 * Convert server-side uid/gid in the posix ACL items to the client-side ones.
 * convert rule:
 * @CFS_IC_NOTHING
 *  nothing to be converted.
 * @CFS_IC_ALL
 *  mapped ids are converted to client-side ones,
 *  unmapped ones are converted to "nobody".
 * @CFS_IC_MAPPED
 *  only mapped ids are converted to "nobody".
 * @CFS_IC_UNMAPPED
 *  only unmapped ids are converted to "nobody".
 */
int lustre_posix_acl_xattr_id2client(struct lu_ucred *mu,
				     struct lustre_idmap_table *t,
				     posix_acl_xattr_header *header,
				     int size, int flags)
{
        int count, i;
        __u32 id;
        ENTRY;

        if (unlikely(size < 0))
                RETURN(-EINVAL);
        else if (!size)
                RETURN(0);

        if (unlikely(flags == CFS_IC_NOTHING))
                RETURN(0);

        count = CFS_ACL_XATTR_COUNT(size, posix_acl_xattr);
        for (i = 0; i < count; i++) {
                id = le32_to_cpu(header->a_entries[i].e_id);
                switch (le16_to_cpu(header->a_entries[i].e_tag)) {
                case ACL_USER_OBJ:
                case ACL_GROUP_OBJ:
                case ACL_MASK:
                case ACL_OTHER:
                        if (id != ACL_UNDEFINED_ID)
                                RETURN(-EIO);
                        break;
                case ACL_USER:
                        id = lustre_idmap_lookup_uid(mu, t, 1, id);
                        if (flags == CFS_IC_ALL) {
                                if (id == CFS_IDMAP_NOTFOUND)
                                        id = NOBODY_UID;
                                header->a_entries[i].e_id = cpu_to_le32(id);
                        } else if (flags == CFS_IC_MAPPED) {
                                if (id != CFS_IDMAP_NOTFOUND)
                                        header->a_entries[i].e_id =
                                                        cpu_to_le32(NOBODY_UID);
                        } else if (flags == CFS_IC_UNMAPPED) {
                                if (id == CFS_IDMAP_NOTFOUND)
                                        header->a_entries[i].e_id =
                                                        cpu_to_le32(NOBODY_UID);
                        }
                        break;
                case ACL_GROUP:
                        id = lustre_idmap_lookup_gid(mu, t, 1, id);
                        if (flags == CFS_IC_ALL) {
                                if (id == CFS_IDMAP_NOTFOUND)
                                        id = NOBODY_GID;
                                header->a_entries[i].e_id = cpu_to_le32(id);
                        } else if (flags == CFS_IC_MAPPED) {
                                if (id != CFS_IDMAP_NOTFOUND)
                                        header->a_entries[i].e_id =
                                                        cpu_to_le32(NOBODY_GID);
                        } else if (flags == CFS_IC_UNMAPPED) {
                                if (id == CFS_IDMAP_NOTFOUND)
                                        header->a_entries[i].e_id =
                                                        cpu_to_le32(NOBODY_GID);
                        }
                        break;
                 default:
                        RETURN(-EIO);
                }
        }
    RETURN(0);
}
EXPORT_SYMBOL(lustre_posix_acl_xattr_id2client);

/*
 * Release the posix ACL space.
 */
void lustre_posix_acl_xattr_free(posix_acl_xattr_header *header, int size)
{
        OBD_FREE(header, size);
}
EXPORT_SYMBOL(lustre_posix_acl_xattr_free);

/*
 * Converts client-side uid/gid in the extended ACL items to server-side ones.
 * convert rule:
 *  mapped ids are converted to server-side ones,
 *  unmapped ones cause "EPERM" error.
 */
int lustre_ext_acl_xattr_id2server(struct lu_ucred *mu,
				   struct lustre_idmap_table *t,
				   ext_acl_xattr_header *header)

{
        int i, count = le32_to_cpu(header->a_count);
        __u32 id;
        ENTRY;

        for (i = 0; i < count; i++) {
                id = le32_to_cpu(header->a_entries[i].e_id);
                switch (le16_to_cpu(header->a_entries[i].e_tag)) {
                case ACL_USER_OBJ:
                case ACL_GROUP_OBJ:
                case ACL_MASK:
                case ACL_OTHER:
                        if (id != ACL_UNDEFINED_ID)
                                RETURN(-EIO);
                        break;
                case ACL_USER:
                        id = lustre_idmap_lookup_uid(mu, t, 0, id);
                        if (id == CFS_IDMAP_NOTFOUND)
                                RETURN(-EPERM);
                        else
                                header->a_entries[i].e_id = cpu_to_le32(id);
                        break;
                case ACL_GROUP:
                        id = lustre_idmap_lookup_gid(mu, t, 0, id);
                        if (id == CFS_IDMAP_NOTFOUND)
                                RETURN(-EPERM);
                        else
                                header->a_entries[i].e_id = cpu_to_le32(id);
                        break;
                default:
                        RETURN(-EIO);
                }
        }
        RETURN(0);
}
EXPORT_SYMBOL(lustre_ext_acl_xattr_id2server);

/*
 * Release the extended ACL space.
 */
void lustre_ext_acl_xattr_free(ext_acl_xattr_header *header)
{
        OBD_FREE(header, CFS_ACL_XATTR_SIZE(le32_to_cpu(header->a_count), \
                                            ext_acl_xattr));
}
EXPORT_SYMBOL(lustre_ext_acl_xattr_free);

static ext_acl_xattr_entry *
lustre_ext_acl_xattr_search(ext_acl_xattr_header *header,
                            posix_acl_xattr_entry *entry, int *pos)
{
        int once, start, end, i, j, count = le32_to_cpu(header->a_count);

        once = 0;
        start = *pos;
        end = count;

again:
        for (i = start; i < end; i++) {
                if (header->a_entries[i].e_tag == entry->e_tag &&
                    header->a_entries[i].e_id == entry->e_id) {
                        j = i;
                        if (++i >= count)
                                i = 0;
                        *pos = i;
                        return &header->a_entries[j];
                }
        }

        if (!once) {
                once = 1;
                start = 0;
                end = *pos;
                goto again;
        }

        return NULL;
}

/*
 * Merge the posix ACL and the extended ACL into new posix ACL.
 */
int lustre_acl_xattr_merge2posix(posix_acl_xattr_header *posix_header, int size,
                                 ext_acl_xattr_header *ext_header,
                                 posix_acl_xattr_header **out)
{
        int posix_count, posix_size, i, j;
        int ext_count = le32_to_cpu(ext_header->a_count), pos = 0, rc = 0;
        posix_acl_xattr_entry pe = {ACL_MASK, 0, ACL_UNDEFINED_ID};
        posix_acl_xattr_header *new;
        ext_acl_xattr_entry *ee, ae;
        ENTRY;

        lustre_posix_acl_cpu_to_le(&pe, &pe);
        ee = lustre_ext_acl_xattr_search(ext_header, &pe, &pos);
        if (ee == NULL || le32_to_cpu(ee->e_stat) == ES_DEL) {
                /* there are only base ACL entries at most. */
                posix_count = 3;
                posix_size = CFS_ACL_XATTR_SIZE(posix_count, posix_acl_xattr);
                OBD_ALLOC(new, posix_size);
                if (unlikely(new == NULL))
                        RETURN(-ENOMEM);

                new->a_version = cpu_to_le32(CFS_ACL_XATTR_VERSION);
                for (i = 0, j = 0; i < ext_count; i++) {
                        lustre_ext_acl_le_to_cpu(&ae,
                                                 &ext_header->a_entries[i]);
                        switch (ae.e_tag) {
                        case ACL_USER_OBJ:
                        case ACL_GROUP_OBJ:
                        case ACL_OTHER:
                                if (ae.e_id != ACL_UNDEFINED_ID)
                                        GOTO(_out, rc = -EIO);

                                if (ae.e_stat != ES_DEL) {
                                        new->a_entries[j].e_tag =
                                                ext_header->a_entries[i].e_tag;
                                        new->a_entries[j].e_perm =
                                                ext_header->a_entries[i].e_perm;
                                        new->a_entries[j++].e_id =
                                                ext_header->a_entries[i].e_id;
                                }
                                break;
                        case ACL_MASK:
                        case ACL_USER:
                        case ACL_GROUP:
                                if (ae.e_stat == ES_DEL)
                                        break;
                        default:
                                GOTO(_out, rc = -EIO);
                        }
                }
        } else {
                /* maybe there are valid ACL_USER or ACL_GROUP entries in the
                 * original server-side ACL, they are regarded as ES_UNC stat.*/
                int ori_posix_count;

                if (unlikely(size < 0))
                        RETURN(-EINVAL);
                else if (!size)
                        ori_posix_count = 0;
                else
                        ori_posix_count =
                                CFS_ACL_XATTR_COUNT(size, posix_acl_xattr);
                posix_count = ori_posix_count + ext_count;
                posix_size =
                        CFS_ACL_XATTR_SIZE(posix_count, posix_acl_xattr);
                OBD_ALLOC(new, posix_size);
                if (unlikely(new == NULL))
                        RETURN(-ENOMEM);

                new->a_version = cpu_to_le32(CFS_ACL_XATTR_VERSION);
                /* 1. process the unchanged ACL entries
                 *    in the original server-side ACL. */
                pos = 0;
                for (i = 0, j = 0; i < ori_posix_count; i++) {
                        ee = lustre_ext_acl_xattr_search(ext_header,
                                        &posix_header->a_entries[i], &pos);
                        if (ee == NULL)
                                memcpy(&new->a_entries[j++],
                                       &posix_header->a_entries[i],
                                       sizeof(posix_acl_xattr_entry));
                }

                /* 2. process the non-deleted entries
                 *    from client-side extended ACL. */
                for (i = 0; i < ext_count; i++) {
                        if (le16_to_cpu(ext_header->a_entries[i].e_stat) !=
                            ES_DEL) {
                                new->a_entries[j].e_tag =
                                                ext_header->a_entries[i].e_tag;
                                new->a_entries[j].e_perm =
                                                ext_header->a_entries[i].e_perm;
                                new->a_entries[j++].e_id =
                                                ext_header->a_entries[i].e_id;
                        }
                }
        }

        /* free unused space. */
        rc = lustre_posix_acl_xattr_reduce_space(&new, posix_count, j);
        if (rc >= 0) {
                posix_size = rc;
                *out = new;
                rc = 0;
        }
        EXIT;

_out:
        if (rc) {
                OBD_FREE(new, posix_size);
                posix_size = rc;
        }
        return posix_size;
}
EXPORT_SYMBOL(lustre_acl_xattr_merge2posix);

/*
 * Merge the posix ACL and the extended ACL into new extended ACL.
 */
ext_acl_xattr_header *
lustre_acl_xattr_merge2ext(posix_acl_xattr_header *posix_header, int size,
                           ext_acl_xattr_header *ext_header)
{
        int ori_ext_count, posix_count, ext_count, ext_size;
        int i, j, pos = 0, rc = 0;
        posix_acl_xattr_entry pae;
        ext_acl_xattr_header *new;
        ext_acl_xattr_entry *ee, eae;
        ENTRY;

        if (unlikely(size < 0))
                RETURN(ERR_PTR(-EINVAL));
        else if (!size)
                posix_count = 0;
        else
                posix_count = CFS_ACL_XATTR_COUNT(size, posix_acl_xattr);
        ori_ext_count = le32_to_cpu(ext_header->a_count);
        ext_count = posix_count + ori_ext_count;
        ext_size = CFS_ACL_XATTR_SIZE(ext_count, ext_acl_xattr);

        OBD_ALLOC(new, ext_size);
        if (unlikely(new == NULL))
                RETURN(ERR_PTR(-ENOMEM));

        for (i = 0, j = 0; i < posix_count; i++) {
                lustre_posix_acl_le_to_cpu(&pae, &posix_header->a_entries[i]);
                switch (pae.e_tag) {
                case ACL_USER_OBJ:
                case ACL_GROUP_OBJ:
                case ACL_MASK:
                case ACL_OTHER:
                        if (pae.e_id != ACL_UNDEFINED_ID)
                                GOTO(out, rc = -EIO);
                case ACL_USER:
                        /* ignore "nobody" entry. */
                        if (pae.e_id == NOBODY_UID)
                                break;

                        new->a_entries[j].e_tag =
                                        posix_header->a_entries[i].e_tag;
                        new->a_entries[j].e_perm =
                                        posix_header->a_entries[i].e_perm;
                        new->a_entries[j].e_id =
                                        posix_header->a_entries[i].e_id;
                        ee = lustre_ext_acl_xattr_search(ext_header,
                                        &posix_header->a_entries[i], &pos);
                        if (ee) {
                                if (posix_header->a_entries[i].e_perm !=
                                                                ee->e_perm)
                                        /* entry modified. */
                                        ee->e_stat =
                                        new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_MOD);
                                else
                                        /* entry unchanged. */
                                        ee->e_stat =
                                        new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_UNC);
                        } else {
                                /* new entry. */
                                new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_ADD);
                        }
                        break;
                case ACL_GROUP:
                        /* ignore "nobody" entry. */
                        if (pae.e_id == NOBODY_GID)
                                break;
                        new->a_entries[j].e_tag =
                                        posix_header->a_entries[i].e_tag;
                        new->a_entries[j].e_perm =
                                        posix_header->a_entries[i].e_perm;
                        new->a_entries[j].e_id =
                                        posix_header->a_entries[i].e_id;
                        ee = lustre_ext_acl_xattr_search(ext_header,
                                        &posix_header->a_entries[i], &pos);
                        if (ee) {
                                if (posix_header->a_entries[i].e_perm !=
                                                                ee->e_perm)
                                        /* entry modified. */
                                        ee->e_stat =
                                        new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_MOD);
                                else
                                        /* entry unchanged. */
                                        ee->e_stat =
                                        new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_UNC);
                        } else {
                                /* new entry. */
                                new->a_entries[j++].e_stat =
                                                        cpu_to_le32(ES_ADD);
                        }
                        break;
                default:
                        GOTO(out, rc = -EIO);
                }
        }

        /* process deleted entries. */
        for (i = 0; i < ori_ext_count; i++) {
                lustre_ext_acl_le_to_cpu(&eae, &ext_header->a_entries[i]);
                if (eae.e_stat == ES_UNK) {
                        /* ignore "nobody" entry. */
                        if ((eae.e_tag == ACL_USER && eae.e_id == NOBODY_UID) ||
                            (eae.e_tag == ACL_GROUP && eae.e_id == NOBODY_GID))
                                continue;

                        new->a_entries[j].e_tag =
                                                ext_header->a_entries[i].e_tag;
                        new->a_entries[j].e_perm =
                                                ext_header->a_entries[i].e_perm;
                        new->a_entries[j].e_id = ext_header->a_entries[i].e_id;
                        new->a_entries[j++].e_stat = cpu_to_le32(ES_DEL);
                }
        }

        new->a_count = cpu_to_le32(j);
        /* free unused space. */
        rc = lustre_ext_acl_xattr_reduce_space(&new, ext_count);
        EXIT;

out:
        if (rc) {
                OBD_FREE(new, ext_size);
                new = ERR_PTR(rc);
        }
        return new;
}
EXPORT_SYMBOL(lustre_acl_xattr_merge2ext);

#endif
