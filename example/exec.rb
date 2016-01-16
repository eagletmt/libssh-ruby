#!/usr/bin/env ruby
require 'libssh'

GC.stress = true
session = LibSSH::Session.new
#session.log_verbosity = :debug
session.host = 'barkhorn'
session.parse_config
session.add_identity('%d/id_ed25519')

session.connect
if session.server_known != LibSSH::SERVER_KNOWN_OK
  raise
end

session.userauth_none
list = session.userauth_list
unless list.include?(:publickey)
  raise "publickey is unsupported: #{list}"
end

if session.userauth_publickey_auto != LibSSH::AUTH_SUCCESS
  raise 'authorization failed'
end
