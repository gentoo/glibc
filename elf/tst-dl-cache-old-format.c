/* Test that ld.so accepts an ld.so.cache written in the old format.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* A cache written by "ldconfig -c old" carries no new-format header, so
   the loader must not look for cache extensions in it.  The test writes
   such a cache, then starts a dynamically linked child, which is where
   an unpatched loader dies before main, and then dlopens a module that
   is only reachable through the cache, which covers the reload path.  */

#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xdlfcn.h>
#include <support/xunistd.h>

/* ldconfig only indexes file names starting with "lib", so the module is
   deployed under a lib-prefixed name and dlopened by that name.  */
#define MOD_BUILT	"tst-dl-path-buf-mod.so"
#define MOD_DEPLOYED	"libtst-dl-path-buf-mod.so"
#define MOD_SYMBOL	"tst_dl_path_buf_mod_value"
#define MOD_EXPECTED	0xaabbccddu

#define MOD_DIR "/tst-dl-cache-old-format"

/* Copied from sysdeps/generic/dl-cache.h, which cannot be included here
   because it relies on libc-internal attributes.  */
#define CACHE_MAGIC_OLD "ld.so-1.7.0"
#define CACHE_MAGIC_NEW "glibc-ld.so.cache1.1"

/* Set in the re-executed child, whose only job is to reach main.  */
static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static void
run_ldconfig (void *closure __attribute__ ((unused)))
{
  char *prog = xasprintf ("%s/ldconfig", support_install_rootsbindir);
  char *args[] = { prog, (char *) "-c", (char *) "old", NULL };
  execv (args[0], args);
  FAIL_EXIT1 ("execv (%s): %m", prog);
}

/* ldconfig accepts an unknown -c argument without complaining and falls
   back to the new format, so verify that the cache on disk really is the
   old format and carries no new-format header anywhere.  */
static void
check_cache_is_old_format (void)
{
  char *path = xasprintf ("%s/ld.so.cache", support_sysconfdir_prefix);
  int fd = xopen (path, O_RDONLY, 0);
  struct stat64 st;
  xfstat64 (fd, &st);
  size_t length = st.st_size;
  char *contents = xmalloc (length);
  xread (fd, contents, length);
  xclose (fd);

  TEST_COMPARE_BLOB (contents, sizeof CACHE_MAGIC_OLD - 1,
		     CACHE_MAGIC_OLD, sizeof CACHE_MAGIC_OLD - 1);
  TEST_VERIFY (memmem (contents, length, CACHE_MAGIC_NEW,
		       sizeof CACHE_MAGIC_NEW - 1) == NULL);

  free (contents);
  free (path);
}

static void
do_prepare (int argc, char **argv)
{
  if (restart)
    return;

  xmkdirp (MOD_DIR, 0777);
  add_temp_file (MOD_DIR);
  char *src = xasprintf ("%s/elf/" MOD_BUILT, support_objdir_root);
  support_copy_file (src, MOD_DIR "/" MOD_DEPLOYED);
  add_temp_file (MOD_DIR "/" MOD_DEPLOYED);
  free (src);

  char *conf = xasprintf ("%s/ld.so.conf", support_sysconfdir_prefix);
  support_write_file_string (conf, MOD_DIR "\n");
  free (conf);

  xmkdirp ("/var/cache/ldconfig", 0777);
  struct support_capture_subprocess result
    = support_capture_subprocess (run_ldconfig, NULL);
  support_capture_subprocess_check (&result, "ldconfig", 0, sc_allow_none);
  support_capture_subprocess_free (&result);

  check_cache_is_old_format ();
}
#define PREPARE do_prepare

static int
do_test (int argc, char **argv)
{
  if (restart)
    /* Reaching main is the entire check: the loader read the old-format
       cache while starting this process.  */
    return 0;

  char *spargv[] = { argv[0], (char *) "--direct", (char *) "--restart",
		     NULL };
  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "restart", 0, sc_allow_none);
  support_capture_subprocess_free (&result);

  /* This process still has the cache from its own startup mapped, so the
     dlopen exercises the reload of the replaced file as well.  */
  void *handle = xdlopen (MOD_DEPLOYED, RTLD_NOW | RTLD_LOCAL);
  unsigned int (*value) (void) = xdlsym (handle, MOD_SYMBOL);
  TEST_COMPARE (value (), MOD_EXPECTED);
  xdlclose (handle);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
