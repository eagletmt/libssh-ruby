require 'spec_helper'

RSpec.describe LibSSH::Channel do
  let(:session) { LibSSH::Session.new }
  let(:channel) { described_class.new(session) }

  before do
    session.host = SshHelper.host
    session.port = DockerHelper.port
    session.user = SshHelper.user
  end

  describe '#open_session' do
    context 'without connected session' do
      it 'raises an error' do
        expect { channel.open_session { :ng } }.to raise_error(ArgumentError)
      end
    end
  end
end
