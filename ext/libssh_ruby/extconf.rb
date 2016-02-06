require 'mkmf'

if ENV['LIBSSH_CFLAGS']
  $CFLAGS = ENV['LIBSSH_CFLAGS']
else
  $CFLAGS << ' -Wall -W'
end

have_header('libssh/libssh.h')
have_library('ssh')
have_const('SSH_KEYTYPE_ED25519', 'libssh/libssh.h')

create_makefile('libssh/libssh_ruby')
