#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
STATUS=$("$DIR"/../service/als-controller -s)
if [ "$STATUS" = "0" ]; then
  "$DIR"/../service/als-controller -e
  notify-send -c "device" -i "$DIR/"'images/active.svg' 'Ambient Light Sensor' 'Enabled'
elif [ "$STATUS" = "1" ]; then
  "$DIR"/../service/als-controller -d
  notify-send -c "device" -i "$DIR/"'images/inactive.svg' 'Ambient Light Sensor' 'Disabled'
else
  echo "Error: $STATUS"
fi
