require "bundler/gem_tasks"
require "rake/extensiontask"

task :build => :compile

Rake::ExtensionTask.new("libssh") do |ext|
  ext.lib_dir = "lib/libssh"
end

task :default => [:clobber, :compile, :spec]
