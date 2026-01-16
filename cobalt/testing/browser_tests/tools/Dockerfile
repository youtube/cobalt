# 1. Specify the Base Image
# This is the starting point of your image. It's like choosing an OS.
# 'ubuntu:latest' is a common choice, but for production, consider
# smaller, more secure images like Google's 'distroless' (go/insecure-docker).
FROM ubuntu:latest

# Install utilities including adb and netcat
RUN apt-get update && apt-get install -y \
    adb \
    netcat-openbsd \
    python3 \
    ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# 2. Set the Working Directory (Optional but good practice)
# This sets the default directory for any subsequent RUN, CMD, ENTRYPOINT, COPY, ADD commands.
WORKDIR /app

# 3. Add and Extract Your Tarball
# Replace 'my_application.tar.gz' with your actual tarball file name.
# Replace '/opt/my_app' with the directory INSIDE the image where you want the contents.
# The 'ADD' command will copy the tarball into the image and
# AUTOMATICALLY extract it because it's a recognized archive format (.tar.gz).
ADD cobalt_browsertests.tar.gz /opt/cobalt_browsertests

# 4. Define Default Command (Optional)
# This command runs when a container is started from this image,
# if no other command is specified. Here, it just starts a bash shell.
CMD ["bash"]
