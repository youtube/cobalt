#!/bin/bash

apt-get clean autoclean
apt-get autoremove -y --purge
rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
rm -rf /var/lib/{apt,cache,log}
echo "Cleaned and purged apt temporaries."
