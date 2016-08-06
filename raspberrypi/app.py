#!/usr/bin/env python3
import asyncio
import aiohttp
import json
import time
import math
import serial
import sys
import pprint


RECEIVE_URL = 'http://192.168.0.26:8000/receive'
AUTH_TOKEN = 'abc123'
TANK_NAME = 'some-tank-name'


@asyncio.coroutine
def send(data):
    """
    Send data
    """
    while aiottp.Timeout(10):



def send_data(measurement, points):
    """

    """
    headers = {
        'Authorization': 'Bearer {}'.format(AUTH_TOKEN),
        'content-type': 'application/json',
    }

    data = {
        "tags": {
            "tank_name": TANK_NAME
        },
        "points": [
            {
                "measurement": measurement,
                "fields": points
            }
        ]
    }

    pprint.pprint(data)
    print('\n\n\n\n')
    #requests.post(RECEIVE_URL, data=json.dumps(data), headers=headers)


def serial_read(ser):
    line = b""
    while True:
        data = ser.read()
        if data == b"\r":
            return line.strip(b'\r').strip(b'\n')
        else:
            line = line + data


def main(ser):

    while True:
        line = serial_read(ser)

        payload = {}


        if line.startswith(b'EC: '):
            ec = line.replace(b'EC: ', b'')
            if ec:
                parts = ec.split(b',')
                data = {
                    'conductivity': float(parts[0]),
                    'tds': float(parts[1]),
                    'salinity': float(parts[2]),
                    'specific_gravity': float(parts[3]),
                }
                send_data('conductivity', data)
        elif line.startswith(b'Temp: '):
            temp = line.replace(b'Temp: ', b'')
            if temp:
                temp = float(temp)
                data = {
                    'farenheit': float("{0:.2f}".format(temp * 1.8 + 32)),
                    'celcius': float(temp)
                }

                send_data('temp', data)
        elif line.startswith(b'PH: '):
            ph = line.replace(b'PH: ', b'')
            if ph:
                send_data('ph', dict(ph=float(ph)))


if __name__ == '__main__':

    try:
        ser = serial.Serial('/dev/ttyACM0', 9600, timeout=0)
        main(ser)
    except KeyboardInterrupt:
        exit()
    except Exception as e:
        print(e)
        raise e
    finally:
        ser.close()

