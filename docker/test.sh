#!/bin/sh
set -eux
make
sudo make install
set +e
if sudo -u postgres make installcheck ; then
  echo Success
  exit 0
else
  cat regression*
  echo Failed
  exit 1
fi
