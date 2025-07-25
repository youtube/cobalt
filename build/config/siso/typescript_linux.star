# -*- bazel-starlark -*-
load("@builtin//struct.star", "module")
load("./config.star", "config")
load("./typescript_all.star", "typescript_all")

__handlers = {}
__handlers.update(typescript_all.handlers)

def __step_config(ctx, step_config):
    remote_run = True
    step_config["input_deps"].update(typescript_all.input_deps)

    # TODO: crbug.com/1478909 - Specify typescript inputs in GN config.
    step_config["input_deps"].update({
        "tools/typescript/ts_definitions.py": [
            "third_party/node/linux/node-linux-x64/bin/node",
            "third_party/node/node_modules:node_modules",
        ],
        "tools/typescript/ts_library.py": [
            "third_party/node/linux/node-linux-x64/bin/node",
            "third_party/node/node.py",
            "third_party/node/node_modules:node_modules",
        ],
    })
    step_config["rules"].extend([
        {
            "name": "typescript/ts_library",
            "command_prefix": "python3 ../../tools/typescript/ts_library.py",
            "inputs": [
                "tools/typescript/ts_library.py",
            ],
            "indirect_inputs": {
                "includes": [
                    "*.js",
                    "*.ts",
                    "*.json",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "remote": remote_run,
            "handler": "typescript_ts_library",
            "output_local": True,
        },
        {
            "name": "typescript/ts_definitions",
            "command_prefix": "python3 ../../tools/typescript/ts_definitions.py",
            "inputs": [
                "tools/typescript/ts_definitions.py",
            ],
            "indirect_inputs": {
                "includes": [
                    "*.ts",  # *.d.ts, *.css.ts, *.html.ts, etc
                    "*.json",
                ],
            },
            "exclude_input_patterns": [
                "*.stamp",
            ],
            "remote": remote_run,
            "handler": "typescript_ts_definitions",
        },
    ])
    return step_config

typescript = module(
    "typescript",
    step_config = __step_config,
    handlers = __handlers,
    filegroups = typescript_all.filegroups,
)
