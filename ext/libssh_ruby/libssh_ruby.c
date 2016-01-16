#include "libssh_ruby.h"

VALUE rb_mLibSSH;

void Init_libssh_ruby(void) {
  rb_mLibSSH = rb_define_module("LibSSH");
#define SERVER(name) \
  rb_define_const(rb_mLibSSH, "SERVER_" #name, INT2FIX(SSH_SERVER_##name))
  SERVER(KNOWN_OK);
  SERVER(KNOWN_CHANGED);
  SERVER(FOUND_OTHER);
  SERVER(NOT_KNOWN);
  SERVER(FILE_NOT_FOUND);
#undef SERVER
#define AUTH(name) \
  rb_define_const(rb_mLibSSH, "AUTH_" #name, INT2FIX(SSH_AUTH_##name))
  AUTH(DENIED);
  AUTH(PARTIAL);
  AUTH(SUCCESS);
  AUTH(AGAIN);
#undef AUTH

  Init_libssh_session();
  Init_libssh_channel();
  Init_libssh_error();
}
