import time


def main(request, response):
    # Sleep to keep the connection pending, allowing the client test to
    # deterministically abort this navigation and trigger a net::ERR_ABORTED
    # early failure without timing races.
    time.sleep(5)
    response.close_connection = True
