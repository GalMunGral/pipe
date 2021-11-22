SOCKS-like Split Proxy (experimental)

To install on Linux (remote)
  /bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/GalMunGral/telescope/master/install/linux.sh)"
To install on MacOS (local)
  /bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/GalMunGral/telescope/master/install/macos.sh)"
In your /etc/hosts:
  127.0.0.1 telescope.remote

./remote [port]
./local [port] [remote-host] [remote-port]

- DNS over proxy is recommended (e.g. socks5h://).
- A more stable implementation: https://github.com/GalMunGral/wormhole
