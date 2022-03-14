make

cp dist/remote /usr/local/bin/telescope-remote
cp telescope-remote.service /etc/systemd/system/

systemctl daemon-reload
systemctl restart telescope-remote
systemctl status telescope-remote

# Ubuntu
sudo ufw allow 3030/tcp
sudo ufw reload
sudo ufw status

