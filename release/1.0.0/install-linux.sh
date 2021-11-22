DIST_DIR=https://raw.githubusercontent.com/GalMunGral/telescope/master/release/1.0.0

curl -fsSLo /usr/local/bin/telescope-remote $DIST_DIR/remote-x86_64-linux-gnu
curl -fsSLo /etc/systemd/system/telescope.service $DIST_DIR/telescope.service

systemctl daemon-reload
systemctl restart telescope
systemctl status telescope
