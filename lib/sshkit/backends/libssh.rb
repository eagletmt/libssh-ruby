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
      private_constant :BUFSIZ

      # @override
      def upload!(local, remote, _options = {})
        with_session do |session|
          scp = LibSSH::Scp.new(session, :write, File.dirname(remote))
          wrap_local_reader(local) do |io, mode|
            scp.init do
              scp.push_file(File.basename(remote), io.size, mode)
              info "Uploading #{remote}"
              while (buf = io.read(BUFSIZ))
                scp.write(buf)
              end
            end
          end
        end
      end

      # @override
      def download!(remote, local, _options = {})
        with_session do |session|
          scp = LibSSH::Scp.new(session, :read, remote)
          scp.init do
            loop do
              case scp.pull_request
              when LibSSH::Scp::REQUEST_NEWFILE
                info "Downloading #{remote}"
                download_file(scp, local)
              when LibSSH::Scp::REQUEST_NEWDIR
                scp.deny_request('Only NEWFILE is acceptable')
              when LibSSH::Scp::REQUEST_EOF
                break
              end
            end
          end
        end
      end

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

      def wrap_local_reader(local)
        if local.respond_to?(:read)
          # local is IO-like object
          yield(local, 0644)
        else
          File.open(local, 'rb') do |f|
            yield(f, File.stat(local).mode & 0xfff)
          end
        end
      end

      def wrap_local_writer(local, mode)
        if local.respond_to?(:write)
          # local is IO-like object
          yield(local)
        else
          File.open(local, 'wb', mode) do |f|
            yield(f)
          end
        end
      end

      def download_file(scp, local)
        size = scp.request_size
        mode = scp.request_permissions
        scp.accept_request
        wrap_local_writer(local, mode) do |io|
          n = 0
          while n < size
            buf = scp.read(size - n)
            io.write(buf)
            n += buf.size
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
            configure_session(session, ssh_options)
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

      def configure_session(session, ssh_options)
        if ssh_options[:port]
          session.port = ssh_options[:port]
        end

        session.add_identity('%d/id_ed25519')
        keys = Array(ssh_options[:keys])
        unless keys.empty?
          keys.each do |key|
            session.add_identity(key)
          end
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
      end
    end
  end
end
