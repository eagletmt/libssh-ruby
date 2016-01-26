#!/usr/bin/env ruby
require 'libssh'
require 'pathname'

host = 'barkhorn'
target_dir = '/tmp'

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

scp = LibSSH::Scp.new(session, :write, target_dir)
scp.init do
  path = Pathname.new(__dir__).parent.join('README.md')
  scp.push_file(path.basename.to_s, path.size, path.stat.mode & 0xfff)
  path.read do |buf|
    scp.write(buf)
  end
  puts "Uploaded #{path} to #{host}:#{File.join(target_dir, path.basename)}"
end
