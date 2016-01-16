#include "libssh_ruby.h"

VALUE rb_eLibSSHError;
ID id_code;

void Init_libssh_error(void)
{
  rb_eLibSSHError = rb_define_class_under(rb_mLibSSH, "Error", rb_eStandardError);
  id_code = rb_intern("code");

  rb_define_attr(rb_eLibSSHError, "code", 1, 0);
}

void libssh_ruby_raise(ssh_session session)
{
  VALUE exc, code, message;
  VALUE argv[1];

  code = INT2FIX(ssh_get_error_code(session));
  message = rb_str_new_cstr(ssh_get_error(session));
  argv[0] = message;
  exc = rb_class_new_instance(1, argv, rb_eLibSSHError);
  rb_ivar_set(exc, id_code, code);

  rb_exc_raise(exc);
}
