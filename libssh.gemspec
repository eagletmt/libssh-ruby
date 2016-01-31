# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'libssh/version'

Gem::Specification.new do |spec|
  spec.name          = 'libssh'
  spec.version       = LibSSH::VERSION
  spec.authors       = ['Kohei Suzuki']
  spec.email         = ['eagletmt@gmail.com']

  spec.summary       = 'Ruby binding for libssh.'
  spec.description   = 'Ruby binding for libssh.'
  spec.homepage      = 'https://github.com/eagletmt/libssh-ruby'
  spec.license       = 'MIT'
  spec.required_ruby_version = '>= 2.2.0'

  spec.files         = `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  spec.bindir        = 'exe'
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ['lib']
  spec.extensions    = ['ext/libssh_ruby/extconf.rb']

  spec.add_development_dependency 'bundler'
  spec.add_development_dependency 'rake'
  spec.add_development_dependency 'rake-compiler'
  spec.add_development_dependency 'rspec', '>= 3.0.0'
  spec.add_development_dependency 'rubocop', '>= 0.36.0'
  spec.add_development_dependency 'sshkit'
  spec.add_development_dependency 'yard'
end
