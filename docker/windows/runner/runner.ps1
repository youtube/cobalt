<<<<<<< HEAD
.\actions-runner\config.cmd --unattended --replace --url https://github.com/${env:RUNNER_REPO} --token $env:RUNNER_TOKEN --name $env:RUNNER_NAME --work $env:RUNNER_WORKDIR --labels $env:RUNNER_LABELS;
.\actions-runner\run.cmd;
=======
C:\actions-runner\config.cmd --unattended --replace --url https://github.com/${env:RUNNER_REPO} --token $env:RUNNER_TOKEN --name $env:RUNNER_NAME --work $env:RUNNER_WORKDIR --ephemeral --labels $env:RUNNER_LABELS;
C:\actions-runner\run.cmd;
>>>>>>> ae22bddb308 (Add --ephemeral option to win runner (#3239))
