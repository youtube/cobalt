### Accessing devtools

Last modified: May 19, 2026

#### **When running loader\_app  as executable**

```shell
# Device:
root@AmlogicFirebolt:/data/gold_cobalt# ./loader_app  --remote-allow-origins=*

# Host
$ adb forward tcp:9222  tcp:9222

#Then open URL http://127.0.0.1:9222/ on browser
```

#### **When running loader\_app as Plugin:**

> **Prerequisite:** Apply [PR #10462](https://github.com/youtube/cobalt/pull/10462) and build with `-DPLUGIN_COBALT_ENABLE_DEVTOOLS=ON` (devel builds only). This patch adds `--remote-debugging-port=9222` and `--remote-allow-origins=*` to `sbmainargs` automatically. Without it, the plugin does not expose the DevTools port and neither ADB nor SSH access will work.

Add `hostToContainer` port forwarding in `/container/cobalt/config.json`:

```json
 "networking": {
            "required": false,
            "data": {
                "type": "nat",
                "ipv6": true,
                "ipv4": true,
                "dnsmasq": true,
                "portForwarding": {
                    "localhostMasquerade": true,
                    "containerToHost": [
                        {
                            "port": 56889,
                            "protocol": "tcp"
                        }
                    ],
                   "hostToContainer": [
                        {
                            "port": 9222,
                            "protocol": "tcp"
                        }
                    ] 
                }
            }
        },
```

Verify port 9222 is listening on the device after Cobalt starts:

```shell
ssh root@192.168.15.112 "netstat -tlnp | grep 9222"
```

Then forward port 9222 to the host using one of the methods below.

##### ADB (USB cable)

```shell
adb forward tcp:9222 tcp:9222
```

##### SSH / Ethernet (no USB cable)

```shell
ssh -fNL 9222:127.0.0.1:9222 root@192.168.15.112
```

Access [http://localhost:9222](http://localhost:9222). In Chrome, go to `chrome://inspect` and add `localhost:9222` as a discovery target.
