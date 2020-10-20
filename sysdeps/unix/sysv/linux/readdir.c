/* Read a directory.  Linux no-LFS version.
   Copyright (C) 2018-2022 Free Software Foundation, Inc.
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

/* Translate the DP64 entry to the non-LFS one in the translation buffer
   at dirstream DS.  Return true is the translation was possible or
   false if either an internal fields can be represented in the non-LFS
   entry or if the translation can not be resized.  */
static bool
dirstream_entry (struct __dirstream *ds, const struct dirent64 *dp64)
{
  off_t d_off = dp64->d_off;
  if (d_off != dp64->d_off)
    return false;
  ino_t d_ino = dp64->d_ino;
  if (d_ino != dp64->d_ino)
    return false;

  /* Expand the translation buffer to hold the new name size.  */
  size_t new_reclen = sizeof (struct dirent)
		    + dp64->d_reclen - offsetof (struct dirent64, d_name);
  if (new_reclen > ds->tbuffer_size)
    {
      char *newbuffer = realloc (ds->tbuffer, new_reclen);
      if (newbuffer == NULL)
	return false;
      ds->tbuffer = newbuffer;
      ds->tbuffer_size = new_reclen;
    }

  struct dirent *dp = (struct dirent *) ds->tbuffer;

  dp->d_off = d_off;
  dp->d_ino = d_ino;
  dp->d_reclen = new_reclen;
  dp->d_type = dp64->d_type;
  memcpy (dp->d_name, dp64->d_name,
	  dp64->d_reclen - offsetof (struct dirent64, d_name));

  return true;
}

/* Read a directory entry from DIRP.  */
struct dirent *
__readdir_unlocked (DIR *dirp)
{
  const int saved_errno = errno;

  while (1)
    {
      if (dirp->offset >= dirp->size)
	{
	  /* We've emptied out our buffer.  Refill it.  */
	  ssize_t bytes = __getdents64 (dirp->fd, dirp->data,
					dirp->allocation);
	  if (bytes <= 0)
	    {
	      /* On some systems getdents fails with ENOENT when the
		 open directory has been rmdir'd already.  POSIX.1
		 requires that we treat this condition like normal EOF.  */
	      if (bytes < 0 && errno == ENOENT)
		bytes = 0;

	      /* Don't modifiy errno when reaching EOF.  */
	      if (bytes == 0)
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
	 buffer can't be resized.  */
      if (dirstream_entry (dirp, dp64))
	{
          dirp->filepos = dp64->d_off;
	  return (struct dirent *) dirp->tbuffer;
	}
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
