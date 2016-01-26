#!/usr/bin/env ruby
require 'shellwords'
require 'sshkit'
require 'sshkit/backends/libssh'
require 'sshkit/dsl'

SSHKit.config.backend = SSHKit::Backend::Libssh
SSHKit.config.backend.configure do |backend|
  backend.pty = ENV.key?('REQUEST_PTY')
  backend.ssh_options[:user] = ENV['SSH_USER']
  if ENV.key?('SSH_PORT')
    backend.ssh_options[:port] = ENV['SSH_PORT'].to_i
  end
end
SSHKit.config.output = SSHKit::Formatter::Pretty.new($stdout)
SSHKit.config.output_verbosity = :debug

on %w[barkhorn rossmann], in: :parallel do |host|
  date = capture(:date)
  puts "#{host}: #{date}"
end

on %w[barkhorn rossmann], in: :parallel do
  execute :ruby, '-e', Shellwords.escape('puts "stdout"; $stderr.puts "stderr"')
  execute :false, raise_on_non_zero_exit: false
end

on %w[barkhorn rossmann], in: :parallel do
  upload! __FILE__, '/tmp/sshkit.rb'
end
