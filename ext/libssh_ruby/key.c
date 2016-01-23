#include "libssh_ruby.h"

VALUE rb_cLibSSHKey;

static void key_free(void *);
static size_t key_memsize(const void *);

static const rb_data_type_t key_type = {
    "ssh_key", {NULL, key_free, key_memsize, {NULL, NULL}},           NULL,
    NULL,      RUBY_TYPED_WB_PROTECTED | RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE key_alloc(VALUE klass) {
  KeyHolder *holder = ALLOC(KeyHolder);
  holder->key = NULL;
  return TypedData_Wrap_Struct(klass, &key_type, holder);
}

static void key_free(void *arg) {
  KeyHolder *holder = arg;
  if (holder->key != NULL) {
    ssh_key_free(holder->key);
  }
  ruby_xfree(holder);
}

static size_t key_memsize(RB_UNUSED_VAR(const void *arg)) {
  return sizeof(KeyHolder);
}

static VALUE m_initialize(VALUE self) {
  KeyHolder *holder;
  TypedData_Get_Struct(self, KeyHolder, &key_type, holder);
  holder->key = ssh_key_new();
  return self;
}

KeyHolder *libssh_ruby_key_holder(VALUE key) {
  KeyHolder *holder;
  TypedData_Get_Struct(key, KeyHolder, &key_type, holder);
  return holder;
}

static VALUE m_sha1(VALUE self) {
  KeyHolder *holder;
  unsigned char *c_hash;
  size_t len;
  VALUE hash;

  holder = libssh_ruby_key_holder(self);
  ssh_get_publickey_hash(holder->key, SSH_PUBLICKEY_HASH_SHA1, &c_hash, &len);
  hash = rb_str_new((char *)c_hash, len);
  ssh_clean_pubkey_hash(&c_hash);

  return hash;
}

static VALUE m_type(VALUE self) {
  return INT2FIX(ssh_key_type(libssh_ruby_key_holder(self)->key));
}

static VALUE m_type_str(VALUE self) {
  return rb_str_new_cstr(
      ssh_key_type_to_char(ssh_key_type(libssh_ruby_key_holder(self)->key)));
}

static VALUE m_public_p(VALUE self) {
  return ssh_key_is_public(libssh_ruby_key_holder(self)->key) ? Qtrue : Qfalse;
}

static VALUE m_private_p(VALUE self) {
  return ssh_key_is_private(libssh_ruby_key_holder(self)->key) ? Qtrue : Qfalse;
}

void Init_libssh_key(void) {
  rb_cLibSSHKey = rb_define_class_under(rb_mLibSSH, "Key", rb_cObject);
  rb_define_alloc_func(rb_cLibSSHKey, key_alloc);

#define E(name) \
  rb_define_const(rb_cLibSSHKey, "KEYTYPE_" #name, INT2FIX(SSH_KEYTYPE_##name))
  E(UNKNOWN);
  E(DSS);
  E(RSA);
  E(RSA1);
  E(ECDSA);
#ifdef HAVE_CONST_SSH_KEYTYPE_ED25519
  E(ED25519);
#endif
#undef E

  rb_define_method(rb_cLibSSHKey, "initialize", RUBY_METHOD_FUNC(m_initialize),
                   0);
  rb_define_method(rb_cLibSSHKey, "sha1", RUBY_METHOD_FUNC(m_sha1), 0);
  rb_define_method(rb_cLibSSHKey, "type", RUBY_METHOD_FUNC(m_type), 0);
  rb_define_method(rb_cLibSSHKey, "type_str", RUBY_METHOD_FUNC(m_type_str), 0);
  rb_define_method(rb_cLibSSHKey, "public?", RUBY_METHOD_FUNC(m_public_p), 0);
  rb_define_method(rb_cLibSSHKey, "private?", RUBY_METHOD_FUNC(m_private_p), 0);
}
