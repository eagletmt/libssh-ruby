#include "libssh_ruby.h"
#include <ruby/thread.h>

#define RAISE_IF_ERROR(rc) \
  if ((rc) == SSH_ERROR)   \
  libssh_ruby_raise(libssh_ruby_session_holder(holder->session)->session)

VALUE rb_cLibSSHScp;

static ID id_read, id_write;

static void scp_mark(void *);
static void scp_free(void *);
static size_t scp_memsize(const void *);

struct ScpHolderStruct {
  ssh_scp scp;
  VALUE session;
};
typedef struct ScpHolderStruct ScpHolder;

static const rb_data_type_t scp_type = {
    "ssh_scp", {scp_mark, scp_free, scp_memsize, {NULL, NULL}},       NULL,
    NULL,      RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE scp_alloc(VALUE klass) {
  ScpHolder *holder = ALLOC(ScpHolder);
  holder->scp = NULL;
  holder->session = Qundef;
  return TypedData_Wrap_Struct(klass, &scp_type, holder);
}

static void scp_mark(void *arg) {
  ScpHolder *holder = arg;
  if (holder->scp != NULL) {
    rb_gc_mark(holder->session);
  }
}

static void scp_free(void *arg) {
  ScpHolder *holder = arg;
  if (holder->scp != NULL) {
    /* XXX: holder->scp must be free'ed before holder->session is free'ed */
    ssh_scp_free(holder->scp);
    holder->scp = NULL;
  }
  ruby_xfree(holder);
}

static size_t scp_memsize(RB_UNUSED_VAR(const void *arg)) {
  return sizeof(ScpHolder);
}

/* @overload initialize(session, mode, path)
 *  Create a new scp session.
 *  @since 0.2.0
 *  @param [Session] session The SSH session to use.
 *  @param [Symbol] mode +:read+ or +:write+.
 *  @param [String] path The directory in which write or read will be done.
 *  @return [Scp]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html ssh_scp_new
 */
static VALUE m_initialize(VALUE self, VALUE session, VALUE mode, VALUE path) {
  ScpHolder *holder;
  SessionHolder *session_holder;
  char *c_path;
  ID id_mode;
  int c_mode;

  Check_Type(mode, T_SYMBOL);
  id_mode = SYM2ID(mode);
  if (id_mode == id_read) {
    c_mode = SSH_SCP_READ;
  } else if (id_mode == id_write) {
    c_mode = SSH_SCP_WRITE;
  } else {
    rb_raise(rb_eArgError, "Invalid mode value: %" PRIsVALUE, mode);
  }
  c_path = StringValueCStr(path);
  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  session_holder = libssh_ruby_session_holder(session);
  holder->scp = ssh_scp_new(session_holder->session, c_mode, c_path);
  holder->session = session;

  return self;
}

struct nogvl_scp_args {
  ssh_scp scp;
  int rc;
};

static void *nogvl_close(void *ptr) {
  struct nogvl_scp_args *args = ptr;
  args->rc = ssh_scp_close(args->scp);
  return NULL;
}

/* @overload close
 *  Close the scp channel.
 *  @since 0.2.0
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html ssh_scp_close
 */
static VALUE m_close(VALUE self) {
  ScpHolder *holder;
  struct nogvl_scp_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  rb_thread_call_without_gvl(nogvl_close, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  return Qnil;
}

static void *nogvl_init(void *ptr) {
  struct nogvl_scp_args *args = ptr;
  args->rc = ssh_scp_init(args->scp);
  return NULL;
}

/* @overload init
 *  Initialize the scp channel.
 *  @since 0.2.0
 *  @yieldparam [Scp] scp self
 *  @return [Object] Return value of the block
 *  @see http://api.libssh.org/stable/group__libssh__scp.html ssh_scp_init
 */
static VALUE m_init(VALUE self) {
  ScpHolder *holder;
  struct nogvl_scp_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  rb_thread_call_without_gvl(nogvl_init, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);

  return rb_ensure(rb_yield, Qnil, m_close, self);
}

struct nogvl_push_file_args {
  ssh_scp scp;
  char *filename;
  uint64_t size;
  int mode;
  int rc;
};

static void *nogvl_push_file(void *ptr) {
  struct nogvl_push_file_args *args = ptr;
  args->rc =
      ssh_scp_push_file64(args->scp, args->filename, args->size, args->mode);
  return NULL;
}

/* @overload push_file(filename, size, mode)
 *  Initialize the sending of a file to a scp in sink mode.
 *  @since 0.2.0
 *  @param [String] filename The name of the file being sent.
 *  @param [Integer] size Exact size in bytes of the file being sent.
 *  @param [Fixnum] mode The UNIX permissions for the new file.
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_push_file64
 */
static VALUE m_push_file(VALUE self, VALUE filename, VALUE size, VALUE mode) {
  ScpHolder *holder;
  struct nogvl_push_file_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  args.filename = StringValueCStr(filename);
  args.size = NUM2ULONG(size);
  args.mode = FIX2INT(mode);
  rb_thread_call_without_gvl(nogvl_push_file, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

struct nogvl_write_args {
  ssh_scp scp;
  const void *buffer;
  size_t len;
  int rc;
};

static void *nogvl_write(void *ptr) {
  struct nogvl_write_args *args = ptr;
  args->rc = ssh_scp_write(args->scp, args->buffer, args->len);
  return NULL;
}

/* @overload write(data)
 *  Write into a remote scp file.
 *  @since 0.2.0
 *  @param [String] data The data to write.
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html ssh_scp_write
 */
static VALUE m_write(VALUE self, VALUE data) {
  ScpHolder *holder;
  struct nogvl_write_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  Check_Type(data, T_STRING);
  args.scp = holder->scp;
  args.buffer = RSTRING_PTR(data);
  args.len = RSTRING_LEN(data);
  rb_thread_call_without_gvl(nogvl_write, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

static void *nogvl_pull_request(void *ptr) {
  struct nogvl_scp_args *args = ptr;
  args->rc = ssh_scp_pull_request(args->scp);
  return NULL;
}

/* @overload pull_request
 *  Wait for a scp request.
 *  @since 0.2.0
 *  @return [Fixnum]
 *    REQUEST_NEWFILE: The other side is sending a file.
 *    REQUEST_NEWDIR: The other side is sending a directory.
 *    REQUEST_ENDDIR: The other side has finished with the current directory.
 *    REQUEST_WARNING: The other side sent us a warning.
 *    REQUEST_EOF: The other side finished sending us files and data.
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_pull_request
 */
static VALUE m_pull_request(VALUE self) {
  ScpHolder *holder;
  struct nogvl_scp_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  rb_thread_call_without_gvl(nogvl_pull_request, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return INT2FIX(args.rc);
}

/* @overload request_size
 *  Get the size of the file being pushed from the other party.
 *  @since 0.2.0
 *  @return [Integer] The numeric size of the file being read.
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_request_get_size64
 */
static VALUE m_request_size(VALUE self) {
  ScpHolder *holder;
  uint64_t size;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  size = ssh_scp_request_get_size64(holder->scp);
  return ULL2NUM(size);
}

/* @overload request_filename
 *  Get the name of the directory or file being pushed from the other party.
 *  @since 0.2.0
 *  @return [String, nil] The filename. +nil+ on error.
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_request_get_filename
 */
static VALUE m_request_filename(VALUE self) {
  ScpHolder *holder;
  const char *filename;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  filename = ssh_scp_request_get_filename(holder->scp);
  if (filename == NULL) {
    return Qnil;
  } else {
    return rb_str_new_cstr(filename);
  }
}

/* @overload request_permissions
 *  Get the permissions of the directory or file being pushed from the other
 *  party.
 *  @since 0.2.0
 *  @return [Fixnum] The UNIX permissions.
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_request_get_permissions
 */
static VALUE m_request_permissions(VALUE self) {
  ScpHolder *holder;
  int mode;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  mode = ssh_scp_request_get_permissions(holder->scp);
  RAISE_IF_ERROR(mode);
  return INT2FIX(mode);
}

static void *nogvl_accept_request(void *ptr) {
  struct nogvl_scp_args *args = ptr;
  args->rc = ssh_scp_accept_request(args->scp);
  return NULL;
}

/* @overload accept_request
 *  Accepts transfer of a file or creation of a directory coming from the remote
 *  party.
 *  @since 0.2.0
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_accept_request
 */
static VALUE m_accept_request(VALUE self) {
  ScpHolder *holder;
  struct nogvl_scp_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  rb_thread_call_without_gvl(nogvl_accept_request, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

struct nogvl_deny_request_args {
  ssh_scp scp;
  const char *reason;
  int rc;
};

static void *nogvl_deny_request(void *ptr) {
  struct nogvl_deny_request_args *args = ptr;
  args->rc = ssh_scp_deny_request(args->scp, args->reason);
  return NULL;
}

/* @overload deny_request
 *  Deny the transfer of a file or creation of a directory coming from the
 *  remote party.
 *  @since 0.2.0
 *  @return [nil]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_deny_request
 */
static VALUE m_deny_request(VALUE self, VALUE reason) {
  ScpHolder *holder;
  struct nogvl_deny_request_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  args.scp = holder->scp;
  args.reason = StringValueCStr(reason);
  rb_thread_call_without_gvl(nogvl_deny_request, &args, RUBY_UBF_IO, NULL);
  RAISE_IF_ERROR(args.rc);
  return Qnil;
}

struct nogvl_read_args {
  ssh_scp scp;
  void *buffer;
  size_t size;
  int rc;
};

static void *nogvl_read(void *ptr) {
  struct nogvl_read_args *args = ptr;
  args->rc = ssh_scp_read(args->scp, args->buffer, args->size);
  return NULL;
}

/* @overload read(size)
 *  Read from a remote scp file.
 *  @since 0.2.0
 *  @param [Fixnum] The size of the buffer.
 *  @return [String]
 *  @see http://api.libssh.org/stable/group__libssh__scp.html ssh_scp_read
 */
static VALUE m_read(VALUE self, VALUE size) {
  ScpHolder *holder;
  struct nogvl_read_args args;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  Check_Type(size, T_FIXNUM);
  args.scp = holder->scp;
  args.size = FIX2INT(size);
  args.buffer = ALLOC_N(char, args.size);
  rb_thread_call_without_gvl(nogvl_read, &args, RUBY_UBF_IO, NULL);
  if (args.rc == SSH_ERROR) {
    ruby_xfree(args.buffer);
    RAISE_IF_ERROR(args.rc);
    return Qnil; /* unreachable */
  } else {
    VALUE ret = rb_utf8_str_new(args.buffer, args.rc);
    ruby_xfree(args.buffer);
    return ret;
  }
}

/* @overload request_warning
 *  Get the warning string.
 *  @since 0.2.0
 *  @return [String, nil] A warning string. +nil+ on error.
 *  @see http://api.libssh.org/stable/group__libssh__scp.html
 *    ssh_scp_request_get_warning
 */
static VALUE m_request_warning(VALUE self) {
  ScpHolder *holder;
  const char *warning;

  TypedData_Get_Struct(self, ScpHolder, &scp_type, holder);
  warning = ssh_scp_request_get_warning(holder->scp);
  if (warning == NULL) {
    return Qnil;
  } else {
    return rb_str_new_cstr(warning);
  }
}

void Init_libssh_scp(void) {
  rb_cLibSSHScp = rb_define_class_under(rb_mLibSSH, "Scp", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHScp, scp_alloc);

  rb_define_const(rb_cLibSSHScp, "REQUEST_NEWFILE",
                  INT2FIX(SSH_SCP_REQUEST_NEWFILE));
  rb_define_const(rb_cLibSSHScp, "REQUEST_NEWDIR",
                  INT2FIX(SSH_SCP_REQUEST_NEWDIR));
  rb_define_const(rb_cLibSSHScp, "REQUEST_ENDDIR",
                  INT2FIX(SSH_SCP_REQUEST_ENDDIR));
  rb_define_const(rb_cLibSSHScp, "REQUEST_WARNING",
                  INT2FIX(SSH_SCP_REQUEST_WARNING));
  rb_define_const(rb_cLibSSHScp, "REQUEST_EOF", INT2FIX(SSH_SCP_REQUEST_EOF));

  rb_define_method(rb_cLibSSHScp, "initialize", RUBY_METHOD_FUNC(m_initialize),
                   3);
  rb_define_method(rb_cLibSSHScp, "init", RUBY_METHOD_FUNC(m_init), 0);
  rb_define_method(rb_cLibSSHScp, "close", RUBY_METHOD_FUNC(m_close), 0);
  rb_define_method(rb_cLibSSHScp, "push_file", RUBY_METHOD_FUNC(m_push_file),
                   3);
  rb_define_method(rb_cLibSSHScp, "write", RUBY_METHOD_FUNC(m_write), 1);

  rb_define_method(rb_cLibSSHScp, "pull_request",
                   RUBY_METHOD_FUNC(m_pull_request), 0);
  rb_define_method(rb_cLibSSHScp, "request_size",
                   RUBY_METHOD_FUNC(m_request_size), 0);
  rb_define_method(rb_cLibSSHScp, "request_filename",
                   RUBY_METHOD_FUNC(m_request_filename), 0);
  rb_define_method(rb_cLibSSHScp, "request_permissions",
                   RUBY_METHOD_FUNC(m_request_permissions), 0);
  rb_define_method(rb_cLibSSHScp, "accept_request",
                   RUBY_METHOD_FUNC(m_accept_request), 0);
  rb_define_method(rb_cLibSSHScp, "deny_request",
                   RUBY_METHOD_FUNC(m_deny_request), 1);
  rb_define_method(rb_cLibSSHScp, "read", RUBY_METHOD_FUNC(m_read), 1);
  rb_define_method(rb_cLibSSHScp, "request_warning",
                   RUBY_METHOD_FUNC(m_request_warning), 0);

  id_read = rb_intern("read");
  id_write = rb_intern("write");
}
