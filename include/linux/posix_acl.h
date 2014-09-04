/*
  File: linux/posix_acl.h

  (C) 2000 Andreas Gruenbacher, <a.gruenbacher@computer.org>
*/


#ifndef __LINUX_POSIX_ACL_H
#define __LINUX_POSIX_ACL_H


#define ACL_UNDEFINED_ID	((unsigned int)-1)

/* a_type field in acl_user_posix_entry_t */
#define ACL_TYPE_ACCESS		(0x8000)
#define ACL_TYPE_DEFAULT	(0x4000)

/* e_tag entry in posix_acl_entry_t */
#define ACL_USER_OBJ		(0x01)
#define ACL_USER		(0x02)
#define ACL_GROUP_OBJ		(0x04)
#define ACL_GROUP		(0x08)
#define ACL_MASK		(0x10)
#define ACL_OTHER		(0x20)

/* permissions in the e_perm field */
#define ACL_READ		(0x04)
#define ACL_WRITE		(0x02)
#define ACL_EXECUTE		(0x01)
//#define ACL_ADD		(0x08)
//#define ACL_DELETE		(0x10)

#ifdef __KERNEL__

typedef struct {
	short			e_tag;
	unsigned short		e_perm;
	unsigned int		e_id;
} posix_acl_entry_t;

typedef struct {
	atomic_t		a_refcount;
	unsigned int		a_count;
	posix_acl_entry_t	a_entries[0];
} posix_acl_t;

#define FOREACH_ACL_ENTRY(pa, acl, pe) \
	for(pa=(acl)->a_entries, pe=pa+(acl)->a_count; pa<pe; pa++)

/* pxacl.c */

extern posix_acl_t *
posix_acl_alloc(
	int count);
extern posix_acl_t *
posix_acl_dup(
	posix_acl_t *acl);
extern posix_acl_t *
posix_acl_clone(
	const posix_acl_t *acl);
extern void
posix_acl_release(
	posix_acl_t *acl);
extern int
posix_acl_valid(
	const posix_acl_t *acl);
extern int
posix_acl_permission(
	struct inode *inode,
	const posix_acl_t *acl,
	int want);
extern posix_acl_t *
posix_acl_from_mode(
	mode_t mode);
extern int
posix_acl_equiv_mode(
	const posix_acl_t *acl,
	mode_t *modep);
extern int
posix_acl_create_masq(
	posix_acl_t *acl,
	mode_t *modep);
extern int
posix_acl_chmod_masq(
	posix_acl_t *acl,
	mode_t mode);
extern int
posix_acl_masq_nfs_v2_mode(
	posix_acl_t *acl,
	mode_t *modep);

#endif  /* __KERNEL__ */


#endif  /* __LINUX_POSIX_ACL_H */
