require 'mkmf'

$CFLAGS << ' -Wall -W'

have_header('libssh/libssh.h')
have_library('ssh')

create_makefile('libssh/libssh_ruby')
