#include "libssh_ruby.h"

#define RAISE_IF_ERROR(rc) \
  if ((rc) == SSH_ERROR)   \
  libssh_ruby_raise(ssh_channel_get_session(holder->channel))

VALUE rb_cLibSSHChannel;

static ID id_stderr, id_timeout;

static void rb_channel_mark(void *);
static void rb_channel_free(void *);
static size_t rb_channel_memsize(const void *);

struct ChannelHolderStruct {
  ssh_channel channel;
  VALUE session;
};
typedef struct ChannelHolderStruct ChannelHolder;

static const rb_data_type_t channel_type = {
    "ssh_channel",
    {rb_channel_mark, rb_channel_free, rb_channel_memsize, {NULL, NULL}},
    NULL,
    NULL,
    RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE channel_alloc(VALUE klass) {
  ChannelHolder *holder = ALLOC(ChannelHolder);
  holder->channel = NULL;
  holder->session = Qundef;
  return TypedData_Wrap_Struct(klass, &channel_type, holder);
}

static void rb_channel_mark(void *arg) {
  ChannelHolder *holder = arg;
  if (holder->channel != NULL) {
    rb_gc_mark(holder->session);
  }
}

static void rb_channel_free(void *arg) {
  ChannelHolder *holder = arg;

  if (holder->channel != NULL) {
    /* XXX: ssh_channel is freed by ssh_session */
    /* ssh_channel_free(holder->channel); */
    holder->channel = NULL;
  }

  ruby_xfree(holder);
}

static size_t rb_channel_memsize(RB_UNUSED_VAR(const void *arg)) {
  return sizeof(ChannelHolder);
}

static VALUE m_initialize(VALUE self, VALUE session) {
  ChannelHolder *holder;
  SessionHolder *session_holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  session_holder = libssh_ruby_session_holder(session);
  holder->channel = ssh_channel_new(session_holder->session);
  holder->session = session;

  return self;
}

static VALUE m_close(VALUE self) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  RAISE_IF_ERROR(ssh_channel_close(holder->channel));

  return Qnil;
}

static VALUE m_open_session(VALUE self) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  RAISE_IF_ERROR(ssh_channel_open_session(holder->channel));

  if (rb_block_given_p()) {
    return rb_ensure(rb_yield, Qnil, m_close, self);
  } else {
    return Qnil;
  }
}

static VALUE m_request_exec(VALUE self, VALUE cmd) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  RAISE_IF_ERROR(
      ssh_channel_request_exec(holder->channel, StringValueCStr(cmd)));

  return Qnil;
}

static VALUE m_read(int argc, VALUE *argv, VALUE self) {
  ChannelHolder *holder;
  VALUE count, opts, is_stderr, timeout;
  const ID table[] = {id_stderr, id_timeout};
  VALUE kwvals[sizeof(table) / sizeof(*table)];
  char *buf;
  uint32_t c_count;
  int nbytes;
  VALUE ret;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  rb_scan_args(argc, argv, "10:", &count, &opts);
  Check_Type(count, T_FIXNUM);
  rb_get_kwargs(opts, table, 0, 2, kwvals);
  if (kwvals[0] == Qundef) {
    is_stderr = Qfalse;
  } else {
    is_stderr = kwvals[0];
  }
  if (kwvals[1] == Qundef) {
    timeout = INT2FIX(-1);
  } else {
    Check_Type(kwvals[1], T_FIXNUM);
    timeout = kwvals[1];
  }

  c_count = FIX2UINT(count);
  buf = ALLOC_N(char, c_count);
  nbytes = ssh_channel_read_timeout(holder->channel, buf, c_count,
                                    RTEST(is_stderr) ? 1 : 0, FIX2INT(timeout));

  ret = rb_utf8_str_new(buf, nbytes);
  ruby_xfree(buf);
  return ret;
}

void Init_libssh_channel(void) {
  rb_cLibSSHChannel = rb_define_class_under(rb_mLibSSH, "Channel", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHChannel, channel_alloc);

  rb_define_method(rb_cLibSSHChannel, "initialize",
                   RUBY_METHOD_FUNC(m_initialize), 1);
  rb_define_method(rb_cLibSSHChannel, "open_session",
                   RUBY_METHOD_FUNC(m_open_session), 0);
  rb_define_method(rb_cLibSSHChannel, "close", RUBY_METHOD_FUNC(m_close), 0);
  rb_define_method(rb_cLibSSHChannel, "request_exec",
                   RUBY_METHOD_FUNC(m_request_exec), 1);
  rb_define_method(rb_cLibSSHChannel, "read", RUBY_METHOD_FUNC(m_read), -1);

  id_stderr = rb_intern("stderr");
  id_timeout = rb_intern("timeout");
}
