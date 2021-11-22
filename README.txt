To install on Linux (remote)
  /bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/GalMunGral/telescope/master/install/linux.sh)"

To install on MacOS (local)
  /bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/GalMunGral/telescope/master/install/macos.sh)"

In your /etc/hosts:
  127.0.0.1 telescope.remote

NOTES:
  - DNS over proxy is recommended (e.g. socks5h://).
  - Previous implementation: https://github.com/GalMunGral/wormhole
