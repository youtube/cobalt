import json
import subprocess


# run this in the main repository

start_commit = '61130122a789864e002d7c32a8c2443fc94a86b6'  # non-inclusive
end_commit = 'c90ba1d7d58ef094c73298b936a522bd48e27687'

# extracts commits from log
result = subprocess.run(['git', 'log', f'{start_commit}..{end_commit}', '--numstat', '--pretty=format:derpderpderp%nhexsha: %H%n%(trailers:key=original-hexsha)'], capture_output=True, text=True)
log = result.stdout.strip()
commits = [x.strip() for x in log.split('derpderpderp')]
commits = commits[1:]

# builds commits_dict and diffs string from commits
commits_dict = {}
diffs = ''
for commit in commits:
    lines = commit.split('\n', 3)
    hexsha = lines[0][len('hexsha: '):]
    og_hexsha = lines[1][len('original-hexsha: '):]
    numstat = lines[3]

    result = subprocess.run(['git', 'show', og_hexsha, '--numstat', '--pretty=format:'], capture_output=True, text=True)
    og_numstat = result.stdout.strip()

    # commits_dict
    commits_dict[hexsha] = {'hexsha': hexsha, 'og_hexsha': og_hexsha, 'numstat': numstat, 'og_numstat': og_numstat, 'is_equal': numstat == og_numstat}

    # diffs string
    if numstat != og_numstat:
        diffs += f'hexsha: {hexsha}\n{numstat}\nog_hexsha: {og_hexsha}\n{og_numstat}\n\n'

# writes numstat.json from commits_dict
with open('numstat.json', 'w') as f:
    json.dump(commits_dict, f, indent=4)

# writes diff.txt from diffs string 
with open('diff.txt', 'w') as f:
    f.write(diffs)
