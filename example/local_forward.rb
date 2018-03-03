#!/usr/bin/env ruby
require 'libssh'
require 'socket'
GC.stress = true

ssh_host = 'rossmann'
remote_host = 'localhost'
remote_port = 5432
local_port = 3000

session = LibSSH::Session.new
session.host = ssh_host
session.parse_config
session.add_identity('%d/id_ed25519')

session.connect
if session.server_known != LibSSH::SERVER_KNOWN_OK
  raise 'server unknown'
end
if session.userauth_publickey_auto != LibSSH::AUTH_SUCCESS
  raise 'authorization failed'
end

mutex = Mutex.new
cv = ConditionVariable.new
server_thread = Thread.start do
  bufsiz = 16384
  TCPServer.open(local_port) do |server|
    mutex.synchronize { cv.signal }
    socket = server.accept
    channel = LibSSH::Channel.new(session)
    channel.open_forward(remote_host, remote_port) do
      reader = Thread.start do
        begin
          loop do
            channel.write(socket.readpartial(bufsiz))
          end
        rescue EOFError # rubocop:disable Lint/HandleExceptions
        end
        channel.send_eof
      end

      writer = Thread.start do
        loop do
          size = channel.poll
          if size.nil?
            break
          end
          data = channel.read(size)
          socket.write(data)
        end
      end

      reader.join
      writer.join
    end
  end
end

mutex.synchronize { cv.wait(mutex) }
system('psql', '-h', 'localhost', '-p', local_port.to_s, '-U', 'postgres', '-d', 'postgres', '-c', 'SELECT NOW()')
server_thread.join
