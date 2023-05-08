#!/bin/sh
set -eux
cp -av /workspace/* .
make
sudo make install

if command pg_isready ; then
  timeout 600 bash -c 'until pg_isready; do sleep 10; done'
else
  sleep 30
fi

set +e
if sudo -u postgres make installcheck ; then
  echo Success
  exit 0
else
  cat regression*
  echo Failed
  exit 1
fi
