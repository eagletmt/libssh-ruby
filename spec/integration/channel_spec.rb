require 'spec_helper'

RSpec.describe LibSSH::Channel do
  let(:session) { LibSSH::Session.new }
  let(:channel) { described_class.new(session) }

  before do
    session.host = SshHelper.host
    session.port = DockerHelper.port
    session.user = SshHelper.user
    session.add_identity(SshHelper.identity_path)
  end

  after do
    session.disconnect
  end

  describe '#open_session' do
    context 'without connected session' do
      it 'raises an error' do
        expect { channel.open_session { :ng } }.to raise_error(ArgumentError)
      end
    end

    context 'with valid condition' do
      before do
        session.connect
        session.userauth_publickey_auto
      end

      it 'returns the block result' do
        expect(channel.open_session { :ok }).to eq(:ok)
      end
    end
  end

  describe '#request_exec' do
    before do
      session.connect
      session.userauth_publickey_auto
    end

    context 'with valid condition' do
      it 'succeeds' do
        channel.open_session do
          expect(channel.request_exec('true')).to be_nil
        end
      end

      it "doesn't allocate a TTY" do
        channel.open_session do
          expect(channel.request_exec('tty')).to be_nil
          expect(channel.get_exit_status).to eq(1)
        end
      end

      context 'with #request_pty' do
        it 'allocates a TTY' do
          channel.open_session do
            expect(channel.request_pty).to be_nil
            expect(channel.request_exec('tty')).to be_nil
            expect(channel.get_exit_status).to eq(0)
          end
        end
      end
    end
  end

  describe '#read_nonblocking' do
    before do
      session.connect
      session.userauth_publickey_auto
    end

    context 'with valid condition' do
      it 'returns stdout' do
        channel.open_session do
          channel.request_exec('echo hello')
          stdout = ''
          stderr = ''
          until channel.eof?
            r = channel.read_nonblocking(64)
            if r
              stdout << r
            end
            r = channel.read_nonblocking(64, true)
            if r
              stderr << r
            end
          end
          expect(stdout).to eq("hello\n")
          expect(stderr).to eq('')
        end
      end

      it 'returns stderr' do
        channel.open_session do
          channel.request_exec('echo hello >&2')
          stdout = ''
          stderr = ''
          until channel.eof?
            r = channel.read_nonblocking(64)
            if r
              stdout << r
            end
            r = channel.read_nonblocking(64, true)
            if r
              stderr << r
            end
          end
          expect(stdout).to eq('')
          expect(stderr).to eq("hello\n")
        end
      end
    end
  end
end
