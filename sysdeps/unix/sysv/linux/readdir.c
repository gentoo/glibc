/* Read a directory.  Linux no-LFS version.
   Copyright (C) 2018-2023 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dirent.h>

#if !_DIRENT_MATCHES_DIRENT64
#include <dirstream.h>

/* Translate the DP64 entry to the non-LFS one in the translation entry
   at dirstream DS.  Return true is the translation was possible or
   false if either an internal field can not be represented in the non-LFS
   entry or if the name is too long.  */
static bool
dirstream_entry (struct __dirstream *ds, const struct dirent64 *dp64)
{
  /* Check for overflow.  */
  if (!in_off_t_range (dp64->d_off) || !in_ino_t_range (dp64->d_ino))
    return false;

  /* And if name is too large.  */
  if (dp64->d_reclen - offsetof (struct dirent64, d_name) > NAME_MAX)
    return false;

  ds->filepos = dp64->d_off;

  ds->tdp.d_off = dp64->d_off;
  ds->tdp.d_ino = dp64->d_ino;
  ds->tdp.d_reclen = sizeof (struct dirent)
    + dp64->d_reclen - offsetof (struct dirent64, d_name);
  ds->tdp.d_type = dp64->d_type;
  memcpy (ds->tdp.d_name, dp64->d_name,
	  dp64->d_reclen - offsetof (struct dirent64, d_name));

  return true;
}

/* Read a directory entry from DIRP.  */
struct dirent *
__readdir_unlocked (DIR *dirp)
{
  int saved_errno = errno;

  while (1)
    {
      if (dirp->offset >= dirp->size)
	{
	  /* We've emptied out our buffer.  Refill it.  */
	  ssize_t bytes = __getdents64 (dirp->fd, dirp->data,
					dirp->allocation);
	  if (bytes <= 0)
	    {
	      /* Linux may fail with ENOENT on some file systems if the
		 directory inode is marked as dead (deleted).  POSIX
		 treats this as a regular end-of-directory condition, so
		 do not set errno in that case, to indicate success.  */
	      if (bytes < 0 && errno == ENOENT)
		__set_errno (saved_errno);
	      return NULL;
	    }
	  dirp->size = bytes;

 	  /* Reset the offset into the buffer.  */
	  dirp->offset = 0;
 	}

      struct dirent64 *dp64 = (struct dirent64 *) &dirp->data[dirp->offset];
      dirp->offset += dp64->d_reclen;

      /* Skip entries which might overflow d_off/d_ino or if the translation
	 buffer can not be resized.  */
      if (dirstream_entry (dirp, dp64))
	return &dirp->tdp;
    }
}

struct dirent *
__readdir (DIR *dirp)
{
  struct dirent *dp;

#if IS_IN (libc)
  __libc_lock_lock (dirp->lock);
#endif
  dp = __readdir_unlocked (dirp);
#if IS_IN (libc)
  __libc_lock_unlock (dirp->lock);
#endif

  return dp;
}
weak_alias (__readdir, readdir)

#endif
