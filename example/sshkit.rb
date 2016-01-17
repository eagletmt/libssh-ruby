#!/usr/bin/env ruby
require 'sshkit'
require 'sshkit/dsl'
require 'sshkit/backends/libssh'

SSHKit.config.backend = SSHKit::Backend::Libssh

on %w[barkhorn rossmann], in: :parallel do |host|
  date = capture(:date)
  puts "#{host}: #{date}"
end
