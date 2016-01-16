#include "libssh.h"

VALUE rb_mLibssh;

void
Init_libssh(void)
{
  rb_mLibssh = rb_define_module("Libssh");
}
