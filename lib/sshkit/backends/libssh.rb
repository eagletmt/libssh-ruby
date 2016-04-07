require 'libssh'
require 'sshkit/backends/abstract'
require 'sshkit/backends/connection_pool'

# Namespace of sshkit gem.
module SSHKit
  # Namespace of sshkit gem.
  module Backend
    # SSHKit backend class for libssh.
    # @since 0.1.0
    # @see https://github.com/capistrano/sshkit
    class Libssh < Abstract
      # Configuration class compatible with
      # {SSHKit::Backend::Netssh::Configuration}.
      # @since 0.1.0
      class Configuration
        # @!attribute [rw] pty
        #   Allocate a PTY or not. Default is +false+.
        #   @return [Boolean]
        # @!attribute [rw] connection_timeout
        #   Connection timeout in second. Default is +30+.
        #   @return [Fixnum]
        # @!attribute [rw] ssh_options
        #   Various options for libssh. Some of them are compatible with
        #   {SSHKit::Backend::Netssh::Configuration#ssh_options}.
        #   @todo Describe supported options.
        #   @return [Hash]
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

      # Upload a local file to the remote party.
      # @param [String, #read] local Path to the local file to be uploaded.
      #   If +local+ responds to +#read+, +local+ is treated as IO-like object.
      # @param [String] remote Path to the remote file.
      # @since 0.2.0
      # @see SSHKit::Backend::Abstract#upload!.
      # @todo Make +options+ compatible with {SSHKit::Backend::Netssh#upload!}.
      def upload!(local, remote, _options = {})
        with_session do |session|
          scp = LibSSH::Scp.new(session, :write, File.dirname(remote))
          wrap_local_reader(local) do |io, mode|
            scp.init do
              scp.push_file(File.basename(remote), io.size, mode)
              info "Uploading #{remote}"
              begin
                loop do
                  scp.write(io.readpartial(BUFSIZ))
                end
              # rubocop:disable Lint/HandleExceptions
              rescue EOFError
              end
            end
          end
        end
      end

      # Download a remote file to local.
      # @param [String] remote Path to the remote file to be downloaded.
      # @param [String, #write] local Path to the local file.
      #   If +local+ responds to +#write+, +local+ is treated as IO-like object.
      # @since 0.2.0
      # @see SSHKit::Backend::Abstract#download!.
      # @todo Make +options+ compatible with {SSHKit::Backend::Netssh#download!}.
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
        # @!attribute [rw] pool
        #   Connection pool for libssh.
        #   @return [SSHKit::Backend::ConnectionPool]
        attr_accessor :pool

        # Global configuration for {SSHKit::Backend::Netssh}.
        # @return [Configuration]
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
              LibSSH::Channel.select([channel], [], [], nil)

              buf = channel.read_nonblocking(BUFSIZ)
              if buf && !buf.empty?
                cmd.on_stdout(channel, buf)
                output.log_command_data(cmd, :stdout, buf)
              end

              buf = channel.read_nonblocking(BUFSIZ, stderr: true)
              if buf && !buf.empty?
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

      def with_session(&block)
        host.ssh_options = Libssh.config.ssh_options.merge(host.ssh_options || {})
        self.class.pool.with(method(:create_session), String(host.hostname), host.username, host.netssh_options, &block)
      end

      def create_session(hostname, username, ssh_options)
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
