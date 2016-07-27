import json
import time
import math
import serial
import sys
import requests


RECEIVE_URL = 'http://172.16.100.24:8000/receive'
AUTH_TOKEN = 'abc123'
TANK_NAME = 'some-tank-name'


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

    requests.post(RECEIVE_URL, data=json.dumps(data), headers=headers)


def serial_read(ser):

    line = ""
    while True:
        data = ser.read()
        if data == "\r":
            return line.strip('\r').strip('\n')
        else:
            line = line + data


def main(ser):

    while True:
        line = serial_read(ser)
        if line.startswith('EC: '):
            ec = line.replace('EC: ', '')
            if ec:
                parts = ec.split(',')
                data = {
                    'conductivity': float(parts[0]),
                    'tds': float(parts[1]),
                    'salinity': float(parts[2]),
                    'specific_gravity': float(parts[3]),
                }
                send_data('conductivity', data)
        elif line.startswith('Temp: '):
            temp = line.replace('Temp: ', '')
            if temp:
                temp = float(temp)
                data = {
                    'farenheit': temp * 1.8 + 32,
                    'celcius': float(temp)
                }

                send_data('temp', data)
        elif line.startswith('PH: '):
            ph = line.replace('PH: ', '')
            if ph:
                send_data('ph', dict(ph=float(ph)))


if __name__ == '__main__':

    try:
        ser = serial.Serial('/dev/ttyACM0', 9600, timeout=0)
        main(ser)
    except KeyboardInterrupt:
        exit()
    except Exception as e:
        print e
    finally:
        ser.close()

