#ifndef LIBSSH_RUBY_H
#define LIBSSH_RUBY_H 1

/* Don't use deprecated functions */
#define LIBSSH_LEGACY_0_4

#include <ruby/ruby.h>
#include <libssh/libssh.h>

extern VALUE rb_mLibSSH;

void Init_libssh_ruby(void);
void Init_libssh_session(void);
void Init_libssh_channel(void);
void Init_libssh_error(void);

void libssh_ruby_raise(ssh_session session);

struct SessionHolderStruct {
  ssh_session session;
};
typedef struct SessionHolderStruct SessionHolder;

SessionHolder *libssh_ruby_session_holder(VALUE session);

#endif /* LIBSSH_RUBY_H */
