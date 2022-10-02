#! /bash/sh

case "$1" in
  start)
    echo "Starting aesdsocket program"
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
  stop)
    echo "stopping aesdsocket program"
    start-stop-daemon -K -n aesdsocket
    ;;
  *)
   echo "Usage: $0 {start|stop}"
  exit 1
 esac

exit 0

