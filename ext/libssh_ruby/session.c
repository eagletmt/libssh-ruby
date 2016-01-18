#include "libssh_ruby.h"
#include <ruby/thread.h>

#define RAISE_IF_ERROR(rc) \
  if ((rc) == SSH_ERROR) libssh_ruby_raise(holder->session)

VALUE rb_cLibSSHSession;

static ID id_none, id_warn, id_info, id_debug, id_trace;
static ID id_password, id_publickey, id_hostbased, id_interactive,
    id_gssapi_mic;

static void session_mark(void *);
static void session_free(void *);
static size_t session_memsize(const void *);

const rb_data_type_t session_type = {
    "ssh_session",
    {session_mark, session_free, session_memsize, {NULL, NULL}},
    NULL,
    NULL,
    RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FREE_IMMEDIATELY,
};

SessionHolder *libssh_ruby_session_holder(VALUE session) {
  SessionHolder *holder;
  TypedData_Get_Struct(session, SessionHolder, &session_type, holder);
  return holder;
}

static VALUE session_alloc(VALUE klass) {
  SessionHolder *holder = ALLOC(SessionHolder);
  holder->session = NULL;
  return TypedData_Wrap_Struct(klass, &session_type, holder);
}

static void session_mark(RB_UNUSED_VAR(void *arg)) {}

static void session_free(void *arg) {
  SessionHolder *holder = arg;
  if (holder->session != NULL) {
    ssh_free(holder->session);
    holder->session = NULL;
  }
  ruby_xfree(holder);
}

static size_t session_memsize(RB_UNUSED_VAR(const void *arg)) {
  return sizeof(SessionHolder);
}

static VALUE m_initialize(VALUE self) {
  SessionHolder *holder;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  holder->session = ssh_new();
  return self;
}

static VALUE m_set_log_verbosity(VALUE self, VALUE verbosity) {
  ID id_verbosity;
  int c_verbosity;
  SessionHolder *holder;

  Check_Type(verbosity, T_SYMBOL);
  id_verbosity = SYM2ID(verbosity);

  if (id_verbosity == id_none) {
    c_verbosity = SSH_LOG_NONE;
  } else if (id_verbosity == id_warn) {
    c_verbosity = SSH_LOG_WARN;
  } else if (id_verbosity == id_info) {
    c_verbosity = SSH_LOG_INFO;
  } else if (id_verbosity == id_debug) {
    c_verbosity = SSH_LOG_DEBUG;
  } else if (id_verbosity == id_trace) {
    c_verbosity = SSH_LOG_TRACE;
  } else {
    rb_raise(rb_eArgError, "invalid verbosity: %" PRIsVALUE, verbosity);
  }

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  RAISE_IF_ERROR(ssh_options_set(holder->session, SSH_OPTIONS_LOG_VERBOSITY,
                                 &c_verbosity));

  return Qnil;
}

static VALUE m_set_host(VALUE self, VALUE host) {
  SessionHolder *holder;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  RAISE_IF_ERROR(ssh_options_set(holder->session, SSH_OPTIONS_HOST,
                                 StringValueCStr(host)));

  return Qnil;
}

static VALUE m_parse_config(int argc, VALUE *argv, VALUE self) {
  SessionHolder *holder;
  VALUE path;
  char *c_path;

  rb_scan_args(argc, argv, "01", &path);

  if (NIL_P(path)) {
    c_path = NULL;
  } else {
    c_path = StringValueCStr(path);
  }

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  if (ssh_options_parse_config(holder->session, c_path) == 0) {
    return Qtrue;
  } else {
    return Qfalse;
  }
}

static VALUE m_add_identity(VALUE self, VALUE path) {
  SessionHolder *holder;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  RAISE_IF_ERROR(ssh_options_set(holder->session, SSH_OPTIONS_ADD_IDENTITY,
                                 StringValueCStr(path)));

  return Qnil;
}

struct nogvl_session_args {
  ssh_session session;
  int rc;
};

static void *nogvl_connect(void *ptr) {
  struct nogvl_session_args *args = ptr;
  args->rc = ssh_connect(args->session);
  return NULL;
}

static VALUE m_connect(VALUE self) {
  SessionHolder *holder;
  struct nogvl_session_args args;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  args.session = holder->session;
  rb_thread_call_without_gvl(nogvl_connect, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  return Qnil;
}

static VALUE m_server_known(VALUE self) {
  SessionHolder *holder;
  int rc;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  rc = ssh_is_server_known(holder->session);
  RAISE_IF_ERROR(rc);
  return INT2FIX(rc);
}

static VALUE m_userauth_none(VALUE self) {
  SessionHolder *holder;
  int rc;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  rc = ssh_userauth_none(holder->session, NULL);
  RAISE_IF_ERROR(rc);
  return INT2FIX(rc);
}

static VALUE m_userauth_list(VALUE self) {
  SessionHolder *holder;
  int list;
  VALUE ary;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  list = ssh_userauth_list(holder->session, NULL);
  RAISE_IF_ERROR(list);

  ary = rb_ary_new();
  if (list & SSH_AUTH_METHOD_NONE) {
    rb_ary_push(ary, ID2SYM(id_none));
  }
  if (list & SSH_AUTH_METHOD_PASSWORD) {
    rb_ary_push(ary, ID2SYM(id_password));
  }
  if (list & SSH_AUTH_METHOD_PUBLICKEY) {
    rb_ary_push(ary, ID2SYM(id_publickey));
  }
  if (list & SSH_AUTH_METHOD_HOSTBASED) {
    rb_ary_push(ary, ID2SYM(id_hostbased));
  }
  if (list & SSH_AUTH_METHOD_INTERACTIVE) {
    rb_ary_push(ary, ID2SYM(id_interactive));
  }
  if (list & SSH_AUTH_METHOD_GSSAPI_MIC) {
    rb_ary_push(ary, ID2SYM(id_gssapi_mic));
  }
  return ary;
}

static void *nogvl_userauth_publickey_auto(void *ptr) {
  struct nogvl_session_args *args = ptr;
  args->rc = ssh_userauth_publickey_auto(args->session, NULL, NULL);
  return NULL;
}

static VALUE m_userauth_publickey_auto(VALUE self) {
  SessionHolder *holder;
  struct nogvl_session_args args;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  args.session = holder->session;
  rb_thread_call_without_gvl(nogvl_userauth_publickey_auto, &args, RUBY_UBF_IO,
                             NULL);
  return INT2FIX(args.rc);
}

static VALUE m_get_publickey(VALUE self) {
  SessionHolder *holder;
  KeyHolder *key_holder;
  VALUE key;

  TypedData_Get_Struct(self, SessionHolder, &session_type, holder);
  key = rb_obj_alloc(rb_cLibSSHKey);
  key_holder = libssh_ruby_key_holder(key);
  RAISE_IF_ERROR(ssh_get_publickey(holder->session, &key_holder->key));
  return key;
}

static VALUE m_write_knownhost(VALUE self) {
  SessionHolder *holder;

  holder = libssh_ruby_session_holder(self);
  RAISE_IF_ERROR(ssh_write_knownhost(holder->session));
  return Qnil;
}

void Init_libssh_session() {
  rb_cLibSSHSession = rb_define_class_under(rb_mLibSSH, "Session", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHSession, session_alloc);

#define I(name) id_##name = rb_intern(#name)
  I(none);
  I(warn);
  I(info);
  I(debug);
  I(trace);
  I(password);
  I(publickey);
  I(hostbased);
  I(interactive);
  I(gssapi_mic);
#undef I

  rb_define_method(rb_cLibSSHSession, "initialize",
                   RUBY_METHOD_FUNC(m_initialize), 0);
  rb_define_method(rb_cLibSSHSession, "log_verbosity=",
                   RUBY_METHOD_FUNC(m_set_log_verbosity), 1);
  rb_define_method(rb_cLibSSHSession, "host=", RUBY_METHOD_FUNC(m_set_host), 1);
  rb_define_method(rb_cLibSSHSession, "parse_config",
                   RUBY_METHOD_FUNC(m_parse_config), -1);
  rb_define_method(rb_cLibSSHSession, "add_identity",
                   RUBY_METHOD_FUNC(m_add_identity), 1);
  rb_define_method(rb_cLibSSHSession, "connect", RUBY_METHOD_FUNC(m_connect),
                   0);
  rb_define_method(rb_cLibSSHSession, "server_known",
                   RUBY_METHOD_FUNC(m_server_known), 0);
  rb_define_method(rb_cLibSSHSession, "userauth_none",
                   RUBY_METHOD_FUNC(m_userauth_none), 0);
  rb_define_method(rb_cLibSSHSession, "userauth_list",
                   RUBY_METHOD_FUNC(m_userauth_list), 0);
  rb_define_method(rb_cLibSSHSession, "userauth_publickey_auto",
                   RUBY_METHOD_FUNC(m_userauth_publickey_auto), 0);
  rb_define_method(rb_cLibSSHSession, "get_publickey",
                   RUBY_METHOD_FUNC(m_get_publickey), 0);
  rb_define_method(rb_cLibSSHSession, "write_knownhost",
                   RUBY_METHOD_FUNC(m_write_knownhost), 0);
}
