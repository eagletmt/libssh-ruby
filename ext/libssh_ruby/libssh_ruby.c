#include "libssh_ruby.h"

VALUE rb_mLibSSH;

/*
 * @overload version(req_version = 0)
 *  When +req_version+ is given, check if libssh is the required version.
 *  When +req_version+ isn't given, return the libssh version string.
 *
 *  @since 0.2.0
 *  @param [Fixnum] req_version The version required.
 *  @return [String, nil] The libssh version string if it's newer than
 *    +req_version+ .
 *  @see http://api.libssh.org/stable/group__libssh__misc.html
 */
static VALUE m_version(int argc, VALUE *argv, RB_UNUSED_VAR(VALUE self)) {
  VALUE req_version;
  int c_req_version = 0;

  rb_scan_args(argc, argv, "01", &req_version);
  if (!NIL_P(req_version)) {
    Check_Type(req_version, T_FIXNUM);
    c_req_version = FIX2INT(req_version);
  }
  return rb_str_new_cstr(ssh_version(c_req_version));
}

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

  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MAJOR",
                  INT2FIX(LIBSSH_VERSION_MAJOR));
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MINOR",
                  INT2FIX(LIBSSH_VERSION_MINOR));
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MICRO",
                  INT2FIX(LIBSSH_VERSION_MICRO));
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_INT",
                  INT2FIX(LIBSSH_VERSION_INT));
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION",
                  rb_str_new_cstr(SSH_STRINGIFY(LIBSSH_VERSION)));

  rb_define_singleton_method(rb_mLibSSH, "version", RUBY_METHOD_FUNC(m_version),
                             -1);

  Init_libssh_session();
  Init_libssh_channel();
  Init_libssh_error();
  Init_libssh_key();
  Init_libssh_scp();
}
