require 'libssh'
require 'sshkit/backends/abstract'
require 'sshkit/backends/connection_pool'

module SSHKit
  module Backend
    class Libssh < Abstract
      class Configuration
        attr_accessor :pty, :connection_timeout, :ssh_options

        def initialize
          super
          self.pty = false
          self.connection_timeout = 30
          self.ssh_options = {}
        end
      end

      BUFSIZ = 16384

      @pool = SSHKit::Backend::ConnectionPool.new

      class << self
        attr_accessor :pool

        # @override
        def config
          @config ||= Configuration.new
        end
      end

      private

      def execute_command(cmd)
        output.log_command_start(cmd)
        cmd.started = true

        with_session do |session|
          channel = LibSSH::Channel.new(session)
          channel.open_session do
            if Libssh.config.pty
              channel.request_pty
            end
            channel.request_exec(cmd.to_command)
            until channel.eof?
              stdout_avail = channel.poll(timeout: 1)
              if stdout_avail && stdout_avail > 0
                buf = channel.read(BUFSIZ)
                cmd.on_stdout(channel, buf)
                output.log_command_data(cmd, :stdout, buf)
              end

              stderr_avail = channel.poll(stderr: true, timeout: 1)
              if stderr_avail && stderr_avail > 0
                buf = channel.read(BUFSIZ, stderr: true)
                cmd.on_stderr(channel, buf)
                output.log_command_data(cmd, :stderr, buf)
              end
            end

            cmd.exit_status = channel.get_exit_status
            output.log_command_exit(cmd)
          end
        end
      end

      def with_session
        host.ssh_options = Libssh.config.ssh_options.merge(host.ssh_options || {})
        entry = self.class.pool.checkout(String(host.hostname), host.username, host.netssh_options) do |hostname, username, ssh_options|
          LibSSH::Session.new.tap do |session|
            session.host = hostname
            username = ssh_options.fetch(:user, username)
            if username
              session.user = username
            end
            if ssh_options[:port]
              session.port = ssh_options[:port]
            end
            ssh_options.fetch(:config, true).tap do |config|
              case config
              when true
                # Load from default ssh_config
                session.parse_config
              when false, nil
                # Don't load from ssh_config
              else
                # Load from specified path
                session.parse_config(config)
              end
            end
            session.add_identity('%d/id_ed25519')

            session.connect
            if session.server_known != LibSSH::SERVER_KNOWN_OK
              raise 'unknown host'
            end
            if session.userauth_publickey_auto != LibSSH::AUTH_SUCCESS
              raise 'authorization failed'
            end
          end
        end

        begin
          yield entry.connection
        ensure
          self.class.pool.checkin(entry)
        end
      end
    end
  end
end
