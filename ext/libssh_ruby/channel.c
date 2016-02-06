#include "libssh_ruby.h"
#include <ruby/thread.h>

#define RAISE_IF_ERROR(rc) \
  if ((rc) == SSH_ERROR)   \
  libssh_ruby_raise(ssh_channel_get_session(holder->channel))

VALUE rb_cLibSSHChannel;

static ID id_stderr, id_timeout;

static void channel_mark(void *);
static void channel_free(void *);
static size_t channel_memsize(const void *);

struct ChannelHolderStruct {
  ssh_channel channel;
  VALUE session;
};
typedef struct ChannelHolderStruct ChannelHolder;

static const rb_data_type_t channel_type = {
    "ssh_channel",
    {channel_mark, channel_free, channel_memsize, {NULL, NULL}},
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

static void channel_mark(void *arg) {
  ChannelHolder *holder = arg;
  if (holder->channel != NULL) {
    rb_gc_mark(holder->session);
  }
}

static void channel_free(void *arg) {
  ChannelHolder *holder = arg;

  if (holder->channel != NULL) {
    /* XXX: ssh_channel is freed by ssh_session */
    /* ssh_channel_free(holder->channel); */
    holder->channel = NULL;
  }

  ruby_xfree(holder);
}

static size_t channel_memsize(RB_UNUSED_VAR(const void *arg)) {
  return sizeof(ChannelHolder);
}

/* @overload initialize(session)
 *  Initialize a channel from the session.
 *  @param [Session] session
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_new
 */
static VALUE m_initialize(VALUE self, VALUE session) {
  ChannelHolder *holder;
  SessionHolder *session_holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  session_holder = libssh_ruby_session_holder(session);
  holder->channel = ssh_channel_new(session_holder->session);
  holder->session = session;

  return self;
}

struct nogvl_channel_args {
  ssh_channel channel;
  int rc;
};

static void *nogvl_close(void *ptr) {
  struct nogvl_channel_args *args = ptr;
  args->rc = ssh_channel_close(args->channel);
  return NULL;
}

/*
 * @overload close
 *  Close a channel.
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_close
 */
static VALUE m_close(VALUE self) {
  ChannelHolder *holder;
  struct nogvl_channel_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_close, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  return Qnil;
}

static void select_session(ssh_session session) {
  fd_set fds;
  int fd;

  FD_ZERO(&fds);
  fd = ssh_get_fd(session);
  FD_SET(fd, &fds);
  select(fd + 1, &fds, NULL, NULL, NULL);
}

static void *nogvl_open_session(void *ptr) {
  struct nogvl_channel_args *args = ptr;
  ssh_session session = ssh_channel_get_session(args->channel);

  while (1) {
    int blocking = ssh_is_blocking(session);
    ssh_set_blocking(session, 0);
    args->rc = ssh_channel_open_session(args->channel);
    ssh_set_blocking(session, blocking);
    if (args->rc != SSH_AGAIN) {
      break;
    }
    rb_thread_check_ints();
    select_session(session);
  }
  return NULL;
}

/*
 * @overload open_session
 *  Open a session channel, and close it after the block.
 *  @yieldparam [Channel] channel self
 *  @return [Object] Return value of the block
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_open_session
 */
static VALUE m_open_session(VALUE self) {
  ChannelHolder *holder;
  struct nogvl_channel_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  /* When ssh_channel_open_session is called before ssh_connect, libssh would
   * crash :-< */
  if (!ssh_is_connected(ssh_channel_get_session(holder->channel))) {
    rb_raise(rb_eArgError, "Session isn't connected");
  }
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_open_session, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  if (rb_block_given_p()) {
    return rb_ensure(rb_yield, Qnil, m_close, self);
  } else {
    return Qnil;
  }
}

struct nogvl_open_forward_args {
  ssh_channel channel;
  const char *remote_host;
  int remote_port;
  int rc;
};

static void *nogvl_open_forward(void *ptr) {
  struct nogvl_open_forward_args *args = ptr;
  ssh_session session = ssh_channel_get_session(args->channel);

  while (1) {
    int blocking = ssh_is_blocking(session);
    ssh_set_blocking(session, 0);
    args->rc = ssh_channel_open_forward(args->channel, args->remote_host,
                                        args->remote_port, "localhost", 22);
    ssh_set_blocking(session, blocking);
    if (args->rc != SSH_AGAIN) {
      break;
    }
    rb_thread_check_ints();
    select_session(session);
  }
  return NULL;
}

/*
 * @overload open_forward(remote_host, remote_port)
 *  Open a TCP/IP forwarding channel.
 *  @param [String] remote_host The remote host to connected.
 *  @param [Fixnum] remote_port The remote port.
 *  @return [nil]
 *  @since 0.4.0
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_open_forward
 */
static VALUE m_open_forward(VALUE self, VALUE remote_host, VALUE remote_port) {
  ChannelHolder *holder;
  struct nogvl_open_forward_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.remote_host = StringValueCStr(remote_host);
  Check_Type(remote_port, T_FIXNUM);
  args.remote_port = FIX2INT(remote_port);
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_open_forward, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  if (rb_block_given_p()) {
    return rb_ensure(rb_yield, Qnil, m_close, self);
  } else {
    return Qnil;
  }
}

struct nogvl_request_exec_args {
  ssh_channel channel;
  char *cmd;
  int rc;
};

static void *nogvl_request_exec(void *ptr) {
  struct nogvl_request_exec_args *args = ptr;
  args->rc = ssh_channel_request_exec(args->channel, args->cmd);

  return NULL;
}

/*
 * @overload request_exec(cmd)
 *  Run a shell command without an interactive shell.
 *  @param [String] cmd The command to execute
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_request_exec
 */
static VALUE m_request_exec(VALUE self, VALUE cmd) {
  ChannelHolder *holder;
  struct nogvl_request_exec_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  args.cmd = StringValueCStr(cmd);
  rb_thread_call_without_gvl(nogvl_request_exec, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

static void *nogvl_request_pty(void *ptr) {
  struct nogvl_channel_args *args = ptr;
  args->rc = ssh_channel_request_pty(args->channel);
  return NULL;
}

/*
 * @overload request_pty
 *  Request a PTY.
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_request_pty
 */
static VALUE m_request_pty(VALUE self) {
  ChannelHolder *holder;
  struct nogvl_channel_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_request_pty, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

struct nogvl_read_args {
  ssh_channel channel;
  char *buf;
  uint32_t count;
  int is_stderr;
  int timeout;
  int rc;
};

static void *nogvl_read(void *ptr) {
  struct nogvl_read_args *args = ptr;
  args->rc = ssh_channel_read_timeout(args->channel, args->buf, args->count,
                                      args->is_stderr, args->timeout);
  return NULL;
}

/*
 * @overload read(count, stderr: false, timeout: -1)
 *  Read data from a channel.
 *  @param [Fixnum] count The count of bytes to be read.
 *  @param [Boolean] stderr Read from the stderr flow or not.
 *  @param [Fixnum] timeout A timeout in seconds. +-1+ means infinite timeout.
 *  @return [String] Data read from the channel.
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_read_timeout
 */
static VALUE m_read(int argc, VALUE *argv, VALUE self) {
  ChannelHolder *holder;
  VALUE count, opts;
  const ID table[] = {id_stderr, id_timeout};
  VALUE kwvals[sizeof(table) / sizeof(*table)];
  struct nogvl_read_args args;
  VALUE ret;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  rb_scan_args(argc, argv, "10:", &count, &opts);
  Check_Type(count, T_FIXNUM);
  rb_get_kwargs(opts, table, 0, 2, kwvals);
  if (kwvals[0] == Qundef) {
    args.is_stderr = 0;
  } else {
    args.is_stderr = RTEST(kwvals[0]) ? 1 : 0;
  }
  if (kwvals[1] == Qundef) {
    args.timeout = -1;
  } else {
    Check_Type(kwvals[1], T_FIXNUM);
    args.timeout = FIX2INT(kwvals[1]);
  }
  args.channel = holder->channel;
  args.count = FIX2UINT(count);
  args.buf = ALLOC_N(char, args.count);
  rb_thread_call_without_gvl(nogvl_read, &args, RUBY_UBF_IO, NULL);

  ret = rb_utf8_str_new(args.buf, args.rc);
  ruby_xfree(args.buf);
  return ret;
}

struct nogvl_read_nonblocking_args {
  ssh_channel channel;
  char *buf;
  uint32_t count;
  int is_stderr;
  int rc;
};

static void *nogvl_read_nonblocking(void *ptr) {
  struct nogvl_read_nonblocking_args *args = ptr;
  args->rc = ssh_channel_read_nonblocking(args->channel, args->buf, args->count,
                                          args->is_stderr);
  return NULL;
}

/*
 * @overload read_nonblocking(count, is_stderr = false)
 *  Do a nonblocking read on the channel.
 *  @param [Fixnum] count The count of bytes to be read.
 *  @param [Boolean] is_stderr Read from the stderr flow or not.
 *  @return [String, nil] Data read from the channel. +nil+ on EOF.
 *  @since 0.3.0
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_read_nonblocking
 */
static VALUE m_read_nonblocking(int argc, VALUE *argv, VALUE self) {
  ChannelHolder *holder;
  VALUE count, is_stderr;
  struct nogvl_read_nonblocking_args args;
  VALUE ret;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  rb_scan_args(argc, argv, "11", &count, &is_stderr);
  Check_Type(count, T_FIXNUM);
  args.count = FIX2UINT(count);
  if (is_stderr == Qundef) {
    args.is_stderr = 0;
  } else {
    args.is_stderr = RTEST(is_stderr) ? 1 : 0;
  }
  args.buf = ALLOC_N(char, args.count);
  rb_thread_call_without_gvl(nogvl_read_nonblocking, &args, RUBY_UBF_IO, NULL);

  if (args.rc == SSH_EOF) {
    ret = Qnil;
  } else {
    ret = rb_utf8_str_new(args.buf, args.rc);
  }
  ruby_xfree(args.buf);
  return ret;
}

/*
 * @overload eof?
 *  Check if remote has sent an EOF.
 *  @return [Boolean]
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_is_eof
 */
static VALUE m_eof_p(VALUE self) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  return ssh_channel_is_eof(holder->channel) ? Qtrue : Qfalse;
}

/*
 * @overload closed?
 *  Check if the channel is closed or not.
 *  @return [Boolean]
 *  @since 0.4.0
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_is_closed
 */
static VALUE m_closed_p(VALUE self) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  return ssh_channel_is_closed(holder->channel) ? Qtrue : Qfalse;
}

/*
 * @overload open?
 *  Check if the channel is open or not.
 *  @return [Boolean]
 *  @since 0.4.0
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_is_open
 */
static VALUE m_open_p(VALUE self) {
  ChannelHolder *holder;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  return ssh_channel_is_open(holder->channel) ? Qtrue : Qfalse;
}

struct nogvl_poll_args {
  ssh_channel channel;
  int timeout;
  int is_stderr;
  int rc;
};

static void *nogvl_poll(void *ptr) {
  struct nogvl_poll_args *args = ptr;
  args->rc =
      ssh_channel_poll_timeout(args->channel, args->timeout, args->is_stderr);
  return NULL;
}

/*
 * @overload poll(stderr: false, timeout: -1)
 *  Poll a channel for data to read.
 *  @param [Boolean] stderr A boolean to select the stderr stream.
 *  @param [Fixnum] timeout A timeout in milliseconds. A negative value means an
 *    infinite timeout.
 *  @return [Fixnum, nil] The number of bytes available for reading. +nil+ if
 *    timed out.
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_poll_timeout
 */
static VALUE m_poll(int argc, VALUE *argv, VALUE self) {
  ChannelHolder *holder;
  VALUE opts;
  const ID table[] = {id_stderr, id_timeout};
  VALUE kwvals[sizeof(table) / sizeof(*table)];
  struct nogvl_poll_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  rb_scan_args(argc, argv, "00:", &opts);
  rb_get_kwargs(opts, table, 0, 2, kwvals);
  if (kwvals[0] == Qundef) {
    args.is_stderr = 0;
  } else {
    args.is_stderr = RTEST(kwvals[0]) ? 1 : 0;
  }
  if (kwvals[1] == Qundef) {
    args.timeout = -1;
  } else {
    Check_Type(kwvals[1], T_FIXNUM);
    args.timeout = FIX2INT(kwvals[1]);
  }

  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_poll, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  if (args.rc == SSH_EOF) {
    return Qnil;
  } else {
    return INT2FIX(args.rc);
  }
}

static void *nogvl_get_exit_status(void *ptr) {
  struct nogvl_channel_args *args = ptr;
  ssh_session session = ssh_channel_get_session(args->channel);

  while (1) {
    int blocking = ssh_is_blocking(session);
    ssh_set_blocking(session, 0);
    args->rc = ssh_channel_get_exit_status(args->channel);
    if (args->rc != SSH_ERROR) {
      break;
    }
    ssh_set_blocking(session, blocking);
    rb_thread_check_ints();
    select_session(session);
  }
  return NULL;
}

/*
 * @overload get_exit_status
 *  Get the exit status of the channel.
 *  @return [Fixnum, nil] The exit status. +nil+ if no exit status has been
 *    returned.
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_get_exit_status
 */
static VALUE m_get_exit_status(VALUE self) {
  ChannelHolder *holder;
  struct nogvl_channel_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_get_exit_status, &args, RUBY_UBF_IO, NULL);
  if (args.rc == -1) {
    return Qnil;
  } else {
    return INT2FIX(args.rc);
  }
}

struct nogvl_write_args {
  ssh_channel channel;
  const void *data;
  uint32_t len;
  int rc;
};

static void *nogvl_write(void *ptr) {
  struct nogvl_write_args *args = ptr;
  args->rc = ssh_channel_write(args->channel, args->data, args->len);
  return NULL;
}

/*
 * @overload write(data)
 *  Write data on the channel.
 *  @param [String] data Data to write.
 *  @return [Fixnum] The number of bytes written.
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_write
 */
static VALUE m_write(VALUE self, VALUE data) {
  ChannelHolder *holder;
  struct nogvl_write_args args;

  Check_Type(data, T_STRING);
  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  args.data = RSTRING_PTR(data);
  args.len = RSTRING_LEN(data);
  rb_thread_call_without_gvl(nogvl_write, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return INT2FIX(args.rc);
}

static void *nogvl_send_eof(void *ptr) {
  struct nogvl_channel_args *args = ptr;
  args->rc = ssh_channel_send_eof(args->channel);
  return NULL;
}

/*
 * @overload send_eof
 *  Send EOF on the channel.
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__channel.html
 *    ssh_channel_send_eof
 */
static VALUE m_send_eof(VALUE self) {
  ChannelHolder *holder;
  struct nogvl_channel_args args;

  TypedData_Get_Struct(self, ChannelHolder, &channel_type, holder);
  args.channel = holder->channel;
  rb_thread_call_without_gvl(nogvl_send_eof, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

/*
 * Document-class: LibSSH::Channel
 * Wrapper for ssh_channel struct in libssh.
 *
 * @since 0.1.0
 * @see http://api.libssh.org/stable/group__libssh__channel.html
 */

void Init_libssh_channel(void) {
  rb_cLibSSHChannel = rb_define_class_under(rb_mLibSSH, "Channel", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHChannel, channel_alloc);

  rb_define_method(rb_cLibSSHChannel, "initialize",
                   RUBY_METHOD_FUNC(m_initialize), 1);
  rb_define_method(rb_cLibSSHChannel, "open_session",
                   RUBY_METHOD_FUNC(m_open_session), 0);
  rb_define_method(rb_cLibSSHChannel, "open_forward",
                   RUBY_METHOD_FUNC(m_open_forward), 2);
  rb_define_method(rb_cLibSSHChannel, "close", RUBY_METHOD_FUNC(m_close), 0);
  rb_define_method(rb_cLibSSHChannel, "request_exec",
                   RUBY_METHOD_FUNC(m_request_exec), 1);
  rb_define_method(rb_cLibSSHChannel, "request_pty",
                   RUBY_METHOD_FUNC(m_request_pty), 0);
  rb_define_method(rb_cLibSSHChannel, "read", RUBY_METHOD_FUNC(m_read), -1);
  rb_define_method(rb_cLibSSHChannel, "read_nonblocking",
                   RUBY_METHOD_FUNC(m_read_nonblocking), -1);
  rb_define_method(rb_cLibSSHChannel, "poll", RUBY_METHOD_FUNC(m_poll), -1);
  rb_define_method(rb_cLibSSHChannel, "eof?", RUBY_METHOD_FUNC(m_eof_p), 0);
  rb_define_method(rb_cLibSSHChannel, "closed?", RUBY_METHOD_FUNC(m_closed_p), 0);
  rb_define_method(rb_cLibSSHChannel, "open?", RUBY_METHOD_FUNC(m_open_p), 0);
  rb_define_method(rb_cLibSSHChannel, "get_exit_status",
                   RUBY_METHOD_FUNC(m_get_exit_status), 0);
  rb_define_method(rb_cLibSSHChannel, "write", RUBY_METHOD_FUNC(m_write), 1);
  rb_define_method(rb_cLibSSHChannel, "send_eof", RUBY_METHOD_FUNC(m_send_eof),
                   0);

  id_stderr = rb_intern("stderr");
  id_timeout = rb_intern("timeout");
}
