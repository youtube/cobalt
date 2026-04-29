#!/bin/bash

echo "=== Cobalt New Checkout Setup ==="

# Assume we are in the repo root
if [ ! -d ".git" ]; then
    echo "Please run this script from the root of the Cobalt git repository."
    exit 1
fi

read -p "Are you an internal Google user? (y/n): " is_internal

# 1. Set up depot_tools
echo "--- Setting up depot_tools ---"
if [ -d "../tools/depot_tools" ]; then
    echo "depot_tools found at ../tools/depot_tools"
    export PATH="$(pwd)/../tools/depot_tools:$PATH"
else
    read -p "depot_tools not found at ../tools/depot_tools. Press Enter to clone it there..."
    mkdir -p ../tools
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ../tools/depot_tools
    export PATH="$(pwd)/../tools/depot_tools:$PATH"
    echo "Please add the following to your ~/.bashrc manually:"
    echo "export PATH=\"\$(pwd)/../tools/depot_tools:\$PATH\""
    read -p "Press Enter after you have noted this..."
fi

# 2. Add Remotes
echo ""
echo "--- Adding Remotes ---"
git remote add _gclient git@github.com:youtube/cobalt.git 2>/dev/null || echo "Remote _gclient already exists or failed to add."
read -p "Enter your GitHub username to add fork remote: " github_user
git remote add fork git@github.com:"$github_user"/cobalt.git 2>/dev/null || echo "Remote fork already exists or failed to add."

# 3. Set up gclient
echo ""
echo "--- Setting up gclient ---"
cd .. # Go to parent of repo root

if [ "$is_internal" == "y" ]; then
    echo "Configuring for internal user with RBE..."
    gclient config --name=src --unmanaged --custom-var='rbe_instance="projects/cobalt-actions-prod/instances/default_instance"' git@github.com:youtube/cobalt.git
else
    echo "Configuring for external user..."
    gclient config --name=src --unmanaged git@github.com:youtube/cobalt.git
fi

echo "Appending target_os and target_cpu to .gclient..."
echo "target_os = [ 'linux', 'android' ]" >> .gclient
echo "target_cpu = ['x64', 'arm', 'arm64']" >> .gclient

# 4. Run gclient sync
echo ""
echo "--- Running gclient sync ---"
cd src
gclient sync --no-history -r $(git rev-parse @)

# 5. Disable Build Telemetry
echo ""
echo "--- Disabling Build Telemetry ---"
cd ..
build_telemetry opt-out

# 6. Generate Build Files
echo ""
echo "--- Generating Build Files ---"
cd src
cobalt/build/gn.py -p linux-x64x11 -c devel

# 7. Enable pre-commit
echo ""
echo "--- Enabling pre-commit ---"
pre-commit clean
pre-commit install -t pre-commit -t pre-push

# 8. Build
echo ""
echo "--- Building ---"
time autoninja -C out/linux-x64x11_devel cobalt:gn_all

echo ""
echo "Checkout and build setup complete!"
