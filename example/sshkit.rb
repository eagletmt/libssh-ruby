#!/usr/bin/env ruby
require 'shellwords'
require 'sshkit'
require 'sshkit/backends/libssh'
require 'sshkit/dsl'

SSHKit.config.backend = SSHKit::Backend::Libssh
SSHKit.config.output = SSHKit::Formatter::Pretty.new($stdout)
SSHKit.config.output_verbosity = :debug

on %w[barkhorn rossmann], in: :parallel do |host|
  date = capture(:date)
  puts "#{host}: #{date}"
end

on %w[barkhorn rossmann], in: :parallel do |host|
  execute :ruby, '-e', Shellwords.escape('puts "stdout"; $stderr.puts "stderr"')
end
