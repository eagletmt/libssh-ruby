# LibSSH
[![Build Status](https://travis-ci.org/eagletmt/libssh-ruby.svg?branch=master)](https://travis-ci.org/eagletmt/libssh-ruby)

Ruby binding for [libssh](https://www.libssh.org/) .

## Stability

Under development

## Requirement

- libssh >= 0.6.0
    - ed25519 keys support is available from [libssh 0.7.0](https://www.libssh.org/2015/05/11/libssh-0-7-0/).

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'libssh'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install libssh

## Usage

See [example/exec.rb](example/exec.rb) and [example/write.rb](example/write.rb).

### With SSHKit

See [example/sshkit.rb](example/sshkit.rb) .

### With Capistrano

```ruby
require 'sshkit/backends/libssh'
set :sshkit_backend, SSHKit::Backend::Libssh
```

## Development

After checking out the repo, run `bin/setup` to install dependencies. You can also run `bin/console` for an interactive prompt that will allow you to experiment.

To install this gem onto your local machine, run `bundle exec rake install`. To release a new version, update the version number in `version.rb`, and then run `bundle exec rake release`, which will create a git tag for the version, push git commits and tags, and push the `.gem` file to [rubygems.org](https://rubygems.org).

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/eagletmt/libssh-ruby.


## License

The gem is available as open source under the terms of the [MIT License](http://opensource.org/licenses/MIT).

