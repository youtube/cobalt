#!/usr/bin/env bash
DEMO_HOME=~/coeg_demo
rm -rf $DEMO_HOME/content/app/cobalt/lib && \
rm -rf $DEMO_HOME/content/app/cobalt/content && \
mkdir -p $DEMO_HOME/content/app/cobalt/lib && \
mkdir -p $DEMO_HOME/content/app/cobalt/content

cobalt/build/gn.py -p linux-x64x11 -c qa
cobalt/build/gn.py -p evergreen-x64 -c qa

ninja -C ./out/linux-x64x11_qa/ loader_app_install && \
ninja -C ./out/linux-x64x11_qa/ native_target/crashpad_handler
ninja -C ./out/evergreen-x64_qa/ cobalt_install && \
cp out/linux-x64x11_qa/loader_app $DEMO_HOME && \
cp out/linux-x64x11_qa/native_target/crashpad_handler $DEMO_HOME && \
cp -r out/linux-x64x11_qa/content/* $DEMO_HOME/content && \
cp out/evergreen-x64_qa/libcobalt.so $DEMO_HOME/content/app/cobalt/lib && \
cp -r out/evergreen-x64_qa/content/* $DEMO_HOME/content/app/cobalt/content


cat > $DEMO_HOME/content/app/cobalt/manifest.json <<EOF
{
	"manifest_version": 2,
	"name": "Cobalt",
	"description": "Cobalt",
	"version": "1.0.0.0"
}
EOF
