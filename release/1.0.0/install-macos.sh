DIST_DIR=https://raw.githubusercontent.com/GalMunGral/telescope/master/release/1.0.0

curl -fsSLo /usr/local/bin/telescope-local $DIST_DIR/local-arm64-apple-darwin20.3.0
curl -fsSLo /Library/LaunchDaemons/telescope.plist $DIST_DIR/telescope.plist

launchctl load /Library/LaunchDaemons/telescope.plist
launchctl start com.galmungral.telescope
launchctl list | grep telescope
