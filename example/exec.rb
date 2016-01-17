#!/usr/bin/env ruby
require 'libssh'

GC.stress = true
session = LibSSH::Session.new
#session.log_verbosity = :debug
session.host = 'barkhorn'
session.parse_config
session.add_identity('%d/id_ed25519')

session.connect
if session.server_known == LibSSH::SERVER_NOT_KNOWN
  pubkey = session.get_publickey
  print "Connect to #{pubkey.type_str} #{pubkey.sha1_hex} ? (y/N) "
  $stdout.flush
  yesno = $stdin.gets.chomp
  if yesno != 'y'
    raise
  end
  session.write_knownhost
  puts 'Wrote known_hosts'
elsif session.server_known != LibSSH::SERVER_KNOWN_OK
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

bufsiz = 16384

channel = LibSSH::Channel.new(session)
channel.open_session do
  channel.request_exec('ps auxf')
  until channel.eof?
    stdout_avail = channel.poll(timeout: 1)
    if stdout_avail && stdout_avail > 0
      $stdout.write(channel.read(bufsiz))
    end

    stderr_avail = channel.poll(stderr: true, timeout: 1)
    if stderr_avail && stderr_avail > 0
      $stderr.write(channel.read(bufsiz, stderr: true))
    end
  end
end
