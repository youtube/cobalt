"""
This module provides methods to control relays on a National Control Devices
ProXR networkable relay board. Each board can have up to 4 banks of relays,
with each bank containing up to 8 relays. All methods in this module
require the bank number and the relay number within that bank to be specified,
with numbering starting at 1.

This module assumes that devices are wired to the Normal Closed (N.C.) side
of the relays, so that when a relay is OFF the device is receiving power,
and when a relay is ON the device is not receiving power.
"""

from __future__ import with_statement
import socket
from contextlib import contextmanager

__all__ = ['get_status',
           'set_status',
           'powercycle']

PORT = 2101

# Some magic numbers from the manual
# Command completed successfully
COMMAND_OK = chr(85)

# Enter command mode.
START_COMMAND = chr(254)


def READ_RELAY_N_AT_BANK(N):
    """
    Return command code for reading status of relay N in a bank.
    """
    return chr(115 + N)


def TURN_ON_RELAY_N_AT_BANK(N):
    """
    Return command code for turning on relay N in a bank.
    """
    return chr(107 + N)


def TURN_OFF_RELAY_N_AT_BANK(N):
    """
    Return command code for turning off relay N in a bank.
    """
    return chr(99 + N)


@contextmanager
def connected_socket(hostname, port):
    """
    Return a TCP socket connected to (hostname, port). The socket
    will be closed when finished.
    """
    sock = socket.socket()
    sock.connect((hostname, port))
    yield sock
    sock.close()


def get_status(sock, bank, relay):
    """
    Get the status of a relay on a specified bank from an already-connected
    socket.

    Return the current state of the relay: True if on, False if off.
    """
    assert(bank >= 1 and bank <= 4)
    assert(relay >= 1 and relay <= 8)
    sock.send(START_COMMAND + READ_RELAY_N_AT_BANK(relay) + chr(bank))
    # will return 0 or 1 indicating relay state
    return ord(sock.recv(256)) == 1


def set_status(sock, bank, relay, status):
    """
    Set the status of a relay on a specified bank from an already-connected
    socket.

    If status is True, turn on the specified relay. If it is False,
    turn off the specified relay.

    Return the current state of the relay: True if on, False if off.
    """
    assert(bank >= 1 and bank <= 4)
    assert(relay >= 1 and relay <= 8)

    if status:
        cmd = TURN_ON_RELAY_N_AT_BANK(relay)
    else:
        cmd = TURN_OFF_RELAY_N_AT_BANK(relay)
    sock.send(START_COMMAND + cmd + chr(bank))
    res = sock.recv(256)
    if res != COMMAND_OK:
        raise Exception("Command did not succeed, status: %d" % res)
    return get_status(sock, bank, relay)


def powercycle(relay_hostname, bank, relay):
    """
    Cycle the power of a device connected to a relay on a specified bank
    on the board at the given hostname.

    The relay will be turned on and then off, with status checked
    after each operation.

    Return True if successful, False otherwise.
    """
    assert(bank >= 1 and bank <= 4)
    assert(relay >= 1 and relay <= 8)
    try:
        with connected_socket(relay_hostname, PORT) as sock:
            # Turn relay on to power off device
            if not set_status(sock, bank, relay, True):
                return False
            # Turn relay off to power on device
            if set_status(sock, bank, relay, False):
                return False
    except:
        return False
    return True
