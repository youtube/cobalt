#!/bin/sh
# Creates self-signed SSL certificates for use with signing server
openssl genrsa -out host.key 1024
openssl req -new -key host.key -x509 > host.cert
