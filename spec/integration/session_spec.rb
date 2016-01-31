require 'spec_helper'

RSpec.describe LibSSH::Session do
  let(:session) { described_class.new }

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
        expect(session.userauth_list).to match_array([:publickey, :password])
      end
    end
  end

  describe '#userauth_publickey_auto' do
    before do
      session.host = SshHelper.host
      session.port = DockerHelper.port
      session.connect
    end

    context 'without valid private key' do
      it 'is denied' do
        expect(session.userauth_publickey_auto).to eq(LibSSH::AUTH_DENIED)
      end
    end

    context 'with valid condition' do
      before do
        session.user = SshHelper.user
        session.add_identity(SshHelper.identity_path)
      end

      it 'returns available methods' do
        expect(session.userauth_publickey_auto).to eq(LibSSH::AUTH_SUCCESS)
      end
    end
  end
end
