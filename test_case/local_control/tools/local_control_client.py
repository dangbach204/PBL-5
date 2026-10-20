#!/usr/bin/env python3
"""Small PC client for the test_case/local_control ESP32 example."""

import argparse
import json
import sys
import urllib3

import requests


urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


def varint(value):
    result = bytearray()
    while value > 0x7F:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value)
    return bytes(result)


def field(number, wire_type, value):
    return varint((number << 3) | wire_type) + value


def message_field(number, value):
    return field(number, 2, varint(len(value)) + value)


def read_varint(data, offset):
    value = 0
    shift = 0
    while offset < len(data):
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, offset
        shift += 7
        if shift > 63:
            raise ValueError("invalid protobuf varint")
    raise ValueError("truncated protobuf varint")


def read_fields(data):
    offset = 0
    fields = []
    while offset < len(data):
        tag, offset = read_varint(data, offset)
        number, wire_type = tag >> 3, tag & 0x07
        if wire_type == 0:
            value, offset = read_varint(data, offset)
        elif wire_type == 2:
            length, offset = read_varint(data, offset)
            value = data[offset:offset + length]
            offset += length
        else:
            raise ValueError(f"unsupported protobuf wire type {wire_type}")
        fields.append((number, wire_type, value))
    return fields


def session_request():
    # SessionData { sec_ver: SecScheme0, sec0: { msg: S0_Session_Command } }
    sec0_payload = message_field(20, b"")
    return field(2, 0, varint(0)) + message_field(10, sec0_payload)


def control_request(message_type, payload_field, payload=b""):
    return field(1, 0, varint(message_type)) + message_field(payload_field, payload)


def get_property_count_request():
    return control_request(0, 10)


def get_property_request(index=0):
    # CmdGetPropertyValues.indices is a packed repeated uint32 field.
    indices = field(1, 2, varint(1) + varint(index))
    return control_request(4, 12, indices)


def set_property_request(enabled):
    value = json.dumps({"status": enabled}, separators=(",", ":")).encode()
    property_value = field(1, 0, varint(0)) + field(2, 2, varint(len(value)) + value)
    properties = message_field(1, property_value)
    return control_request(6, 14, properties)


def nested_field(data, number):
    for field_number, wire_type, value in read_fields(data):
        if field_number == number and wire_type == 2:
            return value
    raise ValueError(f"protobuf field {number} not found")


def scalar_field(data, number, default=None):
    for field_number, wire_type, value in read_fields(data):
        if field_number == number and wire_type == 0:
            return value
    return default


def check_response(response):
    if response.status_code != 200:
        raise RuntimeError(f"HTTP {response.status_code}: {response.text[:200]}")
    return response.content


class LocalControlClient:
    def __init__(self, host, port=443, verify=False, timeout=5):
        self.base_url = f"https://{host}:{port}"
        self.session = requests.Session()
        self.verify = verify
        self.timeout = timeout

    def post(self, endpoint, body):
        response = self.session.post(
            f"{self.base_url}/esp_local_ctrl/{endpoint}",
            data=body,
            headers={"Content-Type": "application/octet-stream"},
            verify=self.verify,
            timeout=self.timeout,
        )
        return check_response(response)

    def start_session(self):
        self.post("session", session_request())

    def get_count(self):
        response = self.post("control", get_property_count_request())
        payload = nested_field(response, 11)
        status = scalar_field(payload, 1)
        if status != 0:
            raise RuntimeError(f"get property count failed, status={status}")
        return scalar_field(payload, 2, 0)

    def get_status(self):
        response = self.post("control", get_property_request(0))
        payload = nested_field(response, 13)
        status = scalar_field(payload, 1)
        if status != 0:
            raise RuntimeError(f"get property failed, status={status}")
        property_info = nested_field(payload, 2)
        property_value = next(value for number, wire, value in read_fields(property_info)
                              if number == 5 and wire == 2)
        return json.loads(property_value.decode())

    def set_status(self, enabled):
        response = self.post("control", set_property_request(enabled))
        payload = nested_field(response, 15)
        status = scalar_field(payload, 1)
        if status != 0:
            raise RuntimeError(f"set status failed, status={status}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="ESP32 IP address or hostname")
    parser.add_argument("command", choices=("info", "get", "on", "off"))
    parser.add_argument("--port", type=int, default=443)
    parser.add_argument("--verify", action="store_true", help="verify the TLS certificate")
    args = parser.parse_args()

    try:
        client = LocalControlClient(args.host, args.port, verify=args.verify)
        client.start_session()
        if args.command == "info":
            print(f"properties: {client.get_count()}")
        elif args.command == "get":
            print(json.dumps(client.get_status()))
        else:
            client.set_status(args.command == "on")
            print(f"status: {args.command}")
    except (requests.RequestException, RuntimeError, ValueError, StopIteration) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())