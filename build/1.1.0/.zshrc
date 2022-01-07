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