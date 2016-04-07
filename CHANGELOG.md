## 0.3.1 (2016-04-08)
- Fix SSHKit::Backend::Libssh for SSHKit v1.9.0
    - libssh now requires SSHKit v1.9.0 or later.

## 0.3.0 (2016-02-06)
- Add wrapper methods
    - `Session#fd`
    - `Session#read_nonblocking`
    - `Session#disconnect`
- Make `Channel#get_exit_status` and `Channel#open_session` interruptible
- More documentations
- Add integration test

## 0.2.0 (2016-01-30)
- Add many wrapper methods
- Support `upload!` and `download!` as SSHKit backend.

## 0.1.0 (2016-01-24)
- Initial release
