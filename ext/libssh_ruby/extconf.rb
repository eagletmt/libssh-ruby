require 'mkmf'

$CFLAGS << ' -Wall -W -ggdb3'

have_header('libssh/libssh.h')
have_library('ssh')

create_makefile('libssh/libssh_ruby')
