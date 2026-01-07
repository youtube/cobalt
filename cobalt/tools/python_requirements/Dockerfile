FROM python:3.11
RUN pip3 install pip-tools
WORKDIR /code
CMD pip-compile --allow-unsafe --generate-hashes /code/cobalt/tools/requirements.in
