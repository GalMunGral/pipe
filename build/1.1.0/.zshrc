function t-on {
  export {HTTP{,S},ALL}_PROXY=127.0.0.1:3031
  export {http{,s},all}_proxy=127.0.0.1:3031
  export ALL_PROXY=socks5h://127.0.0.1:3030
  export all_proxy=socks5h://127.0.0.1:3030
}

function t-off {
  unset {HTTP{,S},ALL}_PROXY
  unset {http{,s},all}_proxy
}


function ts-on {
  networksetup -setwebproxy wi-fi 127.0.0.1 3031
  networksetup -setwebproxystate wi-fi on

  networksetup -setsecurewebproxy wi-fi 127.0.0.1 3031
  networksetup -setsecurewebproxystate wi-fi on
  
  # networksetup -setsocksfirewallproxy wi-fi 127.0.0.1 3030
  # networksetup -setsocksfirewallproxystate wi-fi on
}

function ts-off {
  networksetup -setwebproxystate wi-fi off
  networksetup -setsecurewebproxystate wi-fi off

  # networksetup -setsocksfirewallproxystate wi-fi off
}

