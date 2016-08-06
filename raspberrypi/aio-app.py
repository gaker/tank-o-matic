
import asyncio
import aiohttp
import json
import serial_asyncio
import serial
import time
import pprint


RECEIVE_URL = 'http://192.168.0.26:8000/receive'
AUTH_TOKEN = 'abc123'
TANK_NAME = 'some-tank-name'


class MySerialTransport(serial_asyncio.SerialTransport):
    """
    Override ``serial_asyncio.SerialTransport._read_ready``


    """
    data = b""

    def __init__(self, loop, protocol, serial_instance):
        super().__init__(loop, protocol, serial_instance)
        self._max_read_size = 1

    def _read_ready(self):
        try:
            data = self._serial.read(self._max_read_size)
        except serial.SerialException as e:
            self._close(exc=e)
        else:
            if data == b"\n":
                out = self.data.strip(b'\r')
                self.data = b""
                self._protocol.data_received(out)
            else:
                self.data = self.data + data


@asyncio.coroutine
def create_serial_connection(loop, protocol_factory, *args, **kwargs):
    ser = serial.serial_for_url(*args, **kwargs)
    protocol = protocol_factory()
    transport = MySerialTransport(loop, protocol, ser)
    return (transport, protocol)



@asyncio.coroutine
def send(measurement, points):

    headers = {
        'Authorization': 'Bearer {}'.format(AUTH_TOKEN),
        'content-type': 'application/json'
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

    print(data)
    yield from aiohttp.post(RECEIVE_URL, data=json.dumps(data), headers=headers)


class SerialOutput(asyncio.Protocol):

    def connection_made(self, transport):
        self.transport = transport
        transport.serial.rts = False

    def data_received(self, line):
        payload = None
        measure = None

        if line.startswith(b"EC: "):
            ec = line.replace(b'EC: ', b'')
            if ec:
                parts = ec.split(b',')
                measure = 'conductivity'
                payload = {
                    'conductivity': float(parts[0]),
                    'tds': float(parts[1]),
                    'salinity': float(parts[2]),
                    'specific_gravity': float(parts[3]),
                }
        elif line.startswith(b'Temp: '):
            temp = line.replace(b'Temp: ', b'')
            if temp:
                temp = float(temp)
                measure = 'temp'
                payload = {
                    'farenheit': float("{0:.2f}".format(temp * 1.8 + 32)),
                    'celcius': float(temp)
                }
        elif line.startswith(b'PH: '):
            ph = line.replace(b'PH: ', b'')
            if ph:
                measure = 'ph'
                payload = dict(ph=float(ph))

        if payload is not None and measure is not None:
            asyncio.async(send(measure, payload))

    def connection_lost(self, exc):
        asyncio.get_event_loop().stop()


if __name__ == '__main__':

    loop = asyncio.get_event_loop()
    coro = create_serial_connection(
        loop, SerialOutput, '/dev/ttyACM0', baudrate=9600)
    loop.run_until_complete(coro)
    loop.run_forever()
    loop.close()

