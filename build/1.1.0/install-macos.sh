sudo mkdir -p /usr/local/bin
sudo cp ./local-arm64-apple-darwin20.3.0 /usr/local/bin/telescope-local
sudo cp ./http-arm64-apple-darwin20.3.0 /usr/local/bin/telescope-http

sudo cp ./telescope-local.plist /Library/LaunchAgents/telescope-local.plist 
sudo cp ./telescope-http.plist /Library/LaunchAgents/telescope-http.plist 
sudo chown root:wheel /Library/LaunchAgents/telescope-*.plist
sudo chmod o-w /Library/LaunchAgents/telescope-*.plist

launchctl load /Library/LaunchAgents/telescope-local.plist
launchctl start com.galmungral.telescope-local
launchctl load /Library/LaunchAgents/telescope-http.plist
launchctl start com.galmungral.telescope-http

launchctl list | grep telescope
