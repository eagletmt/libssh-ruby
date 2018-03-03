require 'spec_helper'

RSpec.describe LibSSH::Session do
  let(:session) { described_class.new }

  after do
    session.disconnect
  end

  describe '#connect' do
    context 'without hostname' do
      it 'raises an error' do
        expect { session.connect }.to raise_error(LibSSH::Error)
      end
    end

    context 'with wrong port number' do
      before do
        session.host = SshHelper.host
        session.port = DockerHelper.port + 1
      end

      it 'raises an error' do
        expect { session.connect }.to raise_error(LibSSH::Error)
      end
    end

    context 'with valid condition' do
      before do
        session.host = SshHelper.host
        session.port = DockerHelper.port
      end

      it 'succeeds' do
        expect(session.connect).to be_nil
      end
    end
  end

  describe '#userauth_list' do
    before do
      session.host = SshHelper.host
      session.port = DockerHelper.port
      session.user = SshHelper.user
      session.connect
    end

    context 'without userauth_none' do
      it 'returns empty' do
        expect(session.userauth_list).to eq([])
      end
    end

    context 'with valid condition' do
      before do
        session.userauth_none
      end

      it 'returns available methods' do
        expect(session.userauth_list).to match_array(%i[publickey password])
      end
    end
  end

  describe '#userauth_publickey_auto' do
    before do
      session.host = SshHelper.host
      session.port = DockerHelper.port
      session.user = SshHelper.user
      session.connect
    end

    context 'without valid private key' do
      it 'is denied' do
        expect(session.userauth_publickey_auto).to eq(LibSSH::AUTH_DENIED)
      end
    end

    context 'with valid condition' do
      before do
        session.add_identity(SshHelper.identity_path)
      end

      it 'returns available methods' do
        expect(session.userauth_publickey_auto).to eq(LibSSH::AUTH_SUCCESS)
      end
    end
  end

  describe '#userauth_password' do
    before do
      session.host = SshHelper.host
      session.port = DockerHelper.port
      session.user = SshHelper.user
      session.connect
    end

    context 'wrong password' do
      it 'is denied' do
        expect(session.userauth_password('12345')).to eq(LibSSH::AUTH_DENIED)
      end
    end

    context 'with valid password' do
      it 'access is granted' do
        expect(session.userauth_password(SshHelper.password)).to eq(LibSSH::AUTH_SUCCESS)
      end
    end
  end

  describe '#server_known' do
    before do
      session.host = SshHelper.host
      session.port = DockerHelper.port
    end

    context 'without known_hosts file' do
      before do
        session.knownhosts = SshHelper.absent_known_hosts
        session.connect
      end

      it 'returns SERVER_NOT_KNOWN' do
        expect(session.server_known).to eq(LibSSH::SERVER_NOT_KNOWN)
      end

      context 'with StrictHostKeyCheck = false' do
        before do
          session.stricthostkeycheck = false
        end

        it 'returns SERVER_KNOWN_OK and writes known_hosts' do
          expect(File).to_not be_readable(SshHelper.absent_known_hosts)
          expect(session.server_known).to eq(LibSSH::SERVER_KNOWN_OK)
          expect(File).to be_readable(SshHelper.absent_known_hosts)
        end
      end
    end

    context 'without known_hosts entry' do
      before do
        session.knownhosts = SshHelper.empty_known_hosts
        session.connect
      end

      it 'returns SERVER_NOT_KNOWN' do
        expect(session.server_known).to eq(LibSSH::SERVER_NOT_KNOWN)
      end

      context 'with StrictHostKeyCheck = false' do
        before do
          session.stricthostkeycheck = false
        end

        it 'returns SERVER_KNOWN_OK' do
          expect(session.server_known).to eq(LibSSH::SERVER_KNOWN_OK)
        end
      end
    end

    context 'with valid known_hosts entry' do
      before do
        session.knownhosts = SshHelper.valid_known_hosts
        session.connect
      end

      it 'returns SERVER_KNOWN_OK' do
        expect(session.server_known).to eq(LibSSH::SERVER_KNOWN_OK)
      end
    end

    context 'with invalid known_hosts entry' do
      before do
        session.knownhosts = SshHelper.invalid_known_hosts
        session.connect
      end

      it 'returns SERVER_KNOWN_CHANGED' do
        expect(session.server_known).to eq(LibSSH::SERVER_KNOWN_CHANGED)
      end
    end
  end
end
