#!/usr/bin/env ruby
require 'libssh'

host = 'barkhorn'
remote_path = '/etc/passwd'

GC.stress = true
session = LibSSH::Session.new
# session.log_verbosity = :debug
session.host = host
session.parse_config
session.add_identity('%d/id_ed25519')

session.connect
if session.server_known != LibSSH::SERVER_KNOWN_OK
  raise
end
if session.userauth_publickey_auto != LibSSH::AUTH_SUCCESS
  raise 'authorization failed'
end

scp = LibSSH::Scp.new(session, :read, remote_path)
scp.init do
  loop do
    case scp.pull_request
    when LibSSH::Scp::REQUEST_NEWFILE
      size = scp.request_size
      puts "size=#{size}, filename=#{scp.request_filename}, mode=#{scp.request_permissions.to_s(8)}"
      scp.accept_request

      n = 0
      buf = String.new
      while n < size
        s = scp.read(size)
        puts "Read #{s.size}"
        n += s.size
        buf << s
      end
      puts buf
    when LibSSH::Scp::REQUEST_NEWDIR
      puts "newdir: name=#{scp.request_filename}, mode=#{scp.request_permissions.to_s(8)}"
      scp.accept_request
    when LibSSH::Scp::REQUEST_ENDDIR
      puts 'enddir'
    when LibSSH::Scp::REQUEST_WARNING
      puts "warning: #{scp.request_warning}"
    when LibSSH::Scp::REQUEST_EOF
      break
    end
  end
end
