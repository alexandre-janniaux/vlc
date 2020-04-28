#
# WARNING:
#
# This file is sourced in bash scripts and included in Makefile.
# It must not use syntax elements other than:
#  - foo=bar (without spaces around = character)
#  - export foo
#  - export foo=bar

# The following symbols do not exist on the minimal macOS / iOS, so they are disabled
# here. This allows compilation also with newer macOS SDKs.
# List assumes macOS 10.10 / iOS 8 at minimum.

# Added symbols in macOS 10.12 / iOS 10 / watchOS 3
export ac_cv_func_basename_r=no
export ac_cv_func_clock_getres=no
export ac_cv_func_clock_gettime=no
export ac_cv_func_clock_settime=no
export ac_cv_func_dirname_r=no
export ac_cv_func_getentropy=no
export ac_cv_func_mkostemp=no
export ac_cv_func_mkostemps=no
export ac_cv_func_timingsafe_bcmp=no

# Added symbols in macOS 10.13 / iOS 11 / watchOS 4 / tvOS 11
export ac_cv_func_open_wmemstream=no
export ac_cv_func_fmemopen=no
export ac_cv_func_open_memstream=no
export ac_cv_func_futimens=no
export ac_cv_func_utimensat=no

# Added symbol in macOS 10.14 / iOS 12 / tvOS 9
export ac_cv_func_thread_get_register_pointer_values=no

# Added symbols in macOS 10.15 / iOS 13 / tvOS 13
export ac_cv_func_aligned_alloc=no
export ac_cv_func_timespec_get=no
