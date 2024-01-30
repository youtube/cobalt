<<<<<<< HEAD
.\actions-runner\config.cmd --unattended --replace --url https://github.com/${env:RUNNER_REPO} --token $env:RUNNER_TOKEN --name $env:RUNNER_NAME --work $env:RUNNER_WORKDIR;
.\actions-runner\run.cmd;
=======
C:\actions-runner\config.cmd --unattended --replace --url https://github.com/${env:RUNNER_REPO} --token $env:RUNNER_TOKEN --name $env:RUNNER_NAME --work $env:RUNNER_WORKDIR --labels $env:RUNNER_LABELS;
C:\actions-runner\run.cmd;
>>>>>>> 7fff8289c3c ([XB1] Add XB1 container for external builds (#2296))
