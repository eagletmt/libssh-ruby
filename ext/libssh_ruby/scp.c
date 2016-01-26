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
 * ssh_scp_push_file64
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

void Init_libssh_scp(void) {
  rb_cLibSSHScp = rb_define_class_under(rb_mLibSSH, "Scp", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHScp, scp_alloc);

  rb_define_method(rb_cLibSSHScp, "initialize", RUBY_METHOD_FUNC(m_initialize),
                   3);
  rb_define_method(rb_cLibSSHScp, "init", RUBY_METHOD_FUNC(m_init), 0);
  rb_define_method(rb_cLibSSHScp, "close", RUBY_METHOD_FUNC(m_close), 0);
  rb_define_method(rb_cLibSSHScp, "push_file", RUBY_METHOD_FUNC(m_push_file),
                   3);
  rb_define_method(rb_cLibSSHScp, "write", RUBY_METHOD_FUNC(m_write), 1);

  id_read = rb_intern("read");
  id_write = rb_intern("write");
}
