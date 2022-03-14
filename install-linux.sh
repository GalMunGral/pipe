make

cp dist/remote /usr/local/bin/telescope-remote
cp telescope-remote.service /etc/systemd/system/

systemctl daemon-reload
systemctl restart telescope-remote
systemctl status telescope-remote

