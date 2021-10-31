---
layout: doc
title: "Cobalt Evergreen Raspi-2 Reference Port"
---
# Cobalt Evergreen Raspi-2 Reference Port

## Requirements

*   Raspberry Pi 2 (image configured per
    [instructions](https://cobalt.dev/development/setup-raspi.html) on
    cobalt.dev)

## Build instructions

```
## Clone the repository
$ git clone https://cobalt.googlesource.com/cobalt

## Build the loader app (new entry point)
$ cd cobalt/src
$ cobalt/build/gyp_cobalt -v raspi-2-sbversion-12 -C qa
$ ninja -C out/raspi-2-sbversion-12_qa loader_app crashpad_handler

## Create package directory for Cobalt Evergreen
$ export COEG_PATH=coeg
$ cp out/raspi-2-sbversion-12_qa/loader_app $COEG_PATH

## Create directory structure for the initial installation
[2-slot configuration]
$ mkdir -p  ~/.cobalt_storage/installation_0/
$ cd  ~/.cobalt_storage/installation_0/

[3-slot configuration]
$ mkdir -p $COEG_PATH/content/app/cobalt
$ cd $COEG_PATH/content/app/cobalt

## Download package
$ curl -L https://dl.google.com/cobalt/evergreen/latest/cobalt_arm-hardfp_qa.crx  -o cobalt.zip

## Unpack content package
$ unzip cobalt.zip
$ rm cobalt.zip
$ cd -
```

The following are the steps to build the Cobalt content that’s contained in the
crx package. Note you only need to do this if you want to build the Cobalt
shared library and supplementary components.

```
## Build Cobalt core locally
$ cd cobalt/src
$ cobalt/build/gyp_cobalt -v evergreen-arm-hardfp-sbversion-12 -C qa
$ ninja -C out/evergreen-arm-hardfp-sbversion-12_qa cobalt

## Copy the generated files to the package directory for Cobalt Evergreen
$ cp -r out/evergreen-arm-hardfp-sbversion-12_qa/lib   $COEG_PATH/content/app/cobalt/
$ cp -r out/evergreen-arm-hardfp-sbversion-12_qa/content   $COEG_PATH/content/app/cobalt/

## Create a file named manifest.json with the following content, and put it under $COEG_PATH/content/app/cobalt/
$ cat > $COEG_PATH/content/app/cobalt/manifest.json <<EOF
{
        "manifest_version": 2,
        "name": "Cobalt",
        "description": "Cobalt",
        "version": "1.0.0"
}
EOF
```

## Deployment instructions

Configure your Raspberry Pi 2 with the following steps from your Linux machine.

```
## Save the address of the device
$ export RASPI_ADDR=<YOUR_RASPI_ID_ADDR>

## Remove old storage directory
$ rm -rf /home/pi/.cobalt_storage

## Copy the Evergreen contents to the device
$ rsync -arvp $COEG_PATH pi@$RASPI_ADDR:/home/pi

## Launch
$ ssh pi@$RASPI_ADDR  /home/pi/$COEG_PATH/loader_app
```

## Run instructions

```
$ ssh pi@$RASPI_ADDR
$ cd coeg
$ ./loader_app
```

Cobalt should load and work as usual, but leveraging Evergreen. That’s it!

## Troubleshooting

### Certificate errors on execution of loader\_app

Certificate issues may occur on certain network environments when launching
`loader_app` via SSH. In this case, try launching with a keyboard directly
connected to the device.

### “Failed to load library at <path>” thrown on startup

The loader can’t find the `libcobalt.so` file. Check that the path to
`libcobalt.so` completely matches the one in Deployment instructions.

### “fatal error: “assert(sk\_file)”” thrown on startup

The loader can’t find the content/data folder or it is malformed. Check that the
path to this folder completely matches the one in Deployment instructions.

### “Check failed: address. Failed to retrieve the address” thrown on startup

Ensure that `libcobalt.so` being used is the correct version. For a rebuild, you
may need to remove the old .cobalt\_storage directory on your device.

```
## Remove old storage directory
$ rm -rf /home/pi/.cobalt_storage
