# server.py

from flask import Flask, jsonify

app = Flask(__name__)

first_req = True

update = {
    "server": "prod",
    "protocol": "3.1",
    "daystart": {
        "elapsed_seconds": 51861,
        "elapsed_days": 6316
    },
    "app": [
        {
            "cohorthint": "SB16",
            "appid": "{A9557415-DDCD-4948-8113-C643EFCF710C}",
            "cohort": "1:3f/3i/3j:",
            "status": "ok",
            "cohortname": "SB16.qa.COBALT",
            "ping": {
                "status": "ok"
            },
            "updatecheck": {
                "status": "ok",
                "urls": {
                    "url": [
                        {
                            "codebase": "https://edgedl.me.gvt1.com/edgedl/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "https://redirector.gvt1.com/edgedl/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "http://edgedl.me.gvt1.com/edgedl/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "http://redirector.gvt1.com/edgedl/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "https://dl.google.com/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "http://dl.google.com/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "https://www.google.com/dl/cobalt/COBALT/99.99.1030/"
                        },
                        {
                            "codebase": "http://www.google.com/dl/cobalt/COBALT/99.99.1030/"
                        }
                    ]
                },
                "manifest": {
                    "version": "99.99.1030",
                    "packages": {
                        "package": [
                            {
                                "hash_sha256": "b43f8d0294ea48c3df44a19f78a354b025a2ab58163c44c1f56ab49327f557db",
                                "size": 16770294,
                                "name": "cobalt_x64_sbversion-16_qa_20240417120115.crx",
                                "fp": "1.b43f8d0294ea48c3df44a19f78a354b025a2ab58163c44c1f56ab49327f557db",
                                "required": True,
                                "hash": "iNAb+q7RCoOGPR5FnoInfXIGSFM\u003d"
                            }
                        ]
                    }
                }
            }
        }
    ]
}

noupdate = {
    "server": "prod",
    "protocol": "3.1",
    "daystart": {
        "elapsed_seconds": 51881,
        "elapsed_days": 6316
    },
    "app": [
        {
            "cohorthint": "SB16.qa.COBALT",
            "appid": "{A9557415-DDCD-4948-8113-C643EFCF710C}",
            "cohort": "1:3f/3i/3j:",
            "status": "ok",
            "cohortname": "SB16.qa.COBALT",
            "ping": {
                "status": "ok"
            },
            "updatecheck": {
                "status": "noupdate"
            }
        }
    ]
}


@app.route('/test/omaha/fake', methods=['POST'])
def get_tasks():
    if first_req:
        return jsonify({'response': update})
    else:
        return jsonify({'response': noupdate})


if __name__ == '__main__':
    app.run(debug=True)
