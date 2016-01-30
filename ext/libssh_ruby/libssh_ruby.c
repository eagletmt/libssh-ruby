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

  /* @see Session#server_known */
  rb_define_const(rb_mLibSSH, "SERVER_KNOWN_OK", INT2FIX(SSH_SERVER_KNOWN_OK));
  /* @see Session#server_known */
  rb_define_const(rb_mLibSSH, "SERVER_KNOWN_CHANGED",
                  INT2FIX(SSH_SERVER_KNOWN_CHANGED));
  /* @see Session#server_known */
  rb_define_const(rb_mLibSSH, "SERVER_FOUND_OTHER",
                  INT2FIX(SSH_SERVER_FOUND_OTHER));
  /* @see Session#server_known */
  rb_define_const(rb_mLibSSH, "SERVER_NOT_KNOWN",
                  INT2FIX(SSH_SERVER_NOT_KNOWN));
  /* @see Session#server_known */
  rb_define_const(rb_mLibSSH, "SERVER_FILE_NOT_FOUND",
                  INT2FIX(SSH_SERVER_FILE_NOT_FOUND));

  /* Return value that indicates an authentication failure. */
  rb_define_const(rb_mLibSSH, "AUTH_DENIED", INT2FIX(SSH_AUTH_DENIED));
  /* Return value that indicates a partially authenticated. */
  rb_define_const(rb_mLibSSH, "AUTH_PARTIAL", INT2FIX(SSH_AUTH_PARTIAL));
  /* Return value that indicates a successful authentication. */
  rb_define_const(rb_mLibSSH, "AUTH_SUCCESS", INT2FIX(SSH_AUTH_SUCCESS));
  /* Return value that indicates EAGAIN in nonblocking mode. */
  rb_define_const(rb_mLibSSH, "AUTH_AGAIN", INT2FIX(SSH_AUTH_AGAIN));

  /* Major version defined in header. */
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MAJOR",
                  INT2FIX(LIBSSH_VERSION_MAJOR));
  /* Minor version defined in header. */
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MINOR",
                  INT2FIX(LIBSSH_VERSION_MINOR));
  /* Micro version defined in header. */
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_MICRO",
                  INT2FIX(LIBSSH_VERSION_MICRO));
  /* Major version defined in header. */
  rb_define_const(rb_mLibSSH, "LIBSSH_VERSION_INT",
                  INT2FIX(LIBSSH_VERSION_INT));
  /* String version defined in header. */
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
