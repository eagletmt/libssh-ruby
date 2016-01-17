require 'libssh'
require 'sshkit/backends/abstract'

module SSHKit
  module Backend
    class Libssh < Abstract
      BUFSIZ = 16384

      private

      def execute_command(cmd)
        output.log_command_start(cmd)
        cmd.started = true

        session = LibSSH::Session.new
        session.host = host.hostname
        session.parse_config
        session.add_identity('%d/id_ed25519')

        session.connect
        if session.server_known != LibSSH::SERVER_KNOWN_OK
          raise 'unknown host'
        end
        if session.userauth_publickey_auto != LibSSH::AUTH_SUCCESS
          raise 'authorization failed'
        end

        channel = LibSSH::Channel.new(session)
        channel.open_session do
          channel.request_exec(cmd.to_command)
          loop do
            # TODO: Read stderr
            buf = channel.read(BUFSIZ)
            if buf.empty?
              # TODO: Detect finish correctly
              break
            end
            cmd.on_stdout(channel, buf)
          end
          # TODO: Set exit status correctly
          cmd.exit_status = 0
          output.log_command_exit(cmd)
        end
      end
    end
  end
end
