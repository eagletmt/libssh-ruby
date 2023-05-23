require 'mkmf'

if ENV['LIBSSH_CFLAGS']
  $CFLAGS = ENV['LIBSSH_CFLAGS']
else
  $CFLAGS << ' -Wall -W'
end

unless have_header('libssh/libssh.h')
  abort 'Cannot find libssh/libssh.h'
end
unless have_library('ssh')
  abort 'Cannot find libssh'
end

# libssh 0.8 has merged libssh_threads into libssh, so it’s not required.
have_library('ssh_threads')

have_const('SSH_KEYTYPE_ED25519', 'libssh/libssh.h')

create_makefile('libssh/libssh_ruby')
