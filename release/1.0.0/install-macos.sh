DIST_DIR=https://raw.githubusercontent.com/GalMunGral/telescope/master/release/1.0.0

# this doesn't seem to work behind a proxy, hmmm...
# curl -SLo /usr/local/bin/telescope-local $DIST_DIR/local-arm64-apple-darwin20.3.0
# curl -SLo /Library/LaunchDaemons/telescope.plist $DIST_DIR/telescope.plist

launchctl load /Library/LaunchDaemons/telescope.plist
launchctl start com.galmungral.telescope
launchctl list | grep telescope

PROXY=socks5h://127.0.0.1:3030

echo "
# telescope-local

function t-on {
  export {HTTP{,S},ALL}_PROXY=$PROXY
  export {http{,s},all}_proxy=$PROXY
}

function t-off {
  unset {HTTP{,S},ALL}_PROXY
  unset {http{,s},all}_proxy
}
" >> ~/.zshrc
