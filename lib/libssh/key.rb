require 'libssh/libssh_ruby'

module LibSSH
  class Key
    def sha1_hex
      sha1.unpack('H*')[0].each_char.each_slice(2).map(&:join).join(':')
    end
  end
end
