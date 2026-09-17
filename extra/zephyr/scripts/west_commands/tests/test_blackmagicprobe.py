# Copyright (c) 2018 Foundries.io
# Copyright (c) 2019 Nordic Semiconductor ASA.
#
# SPDX-License-Identifier: Apache-2.0

import sys

import argparse
from unittest.mock import patch, call, MagicMock
import os

import pytest

import usb
import usb.backend
from types import SimpleNamespace, ModuleType
from functools import reduce

from runners import blackmagicprobe
from runners.blackmagicprobe import BlackMagicProbeRunner
from conftest import RC_KERNEL_ELF, RC_GDB
import serial.tools.list_ports_common

from collections import deque

TEST_GDB_SERIAL = 'test-gdb-serial'
TEST_RTT_SERIAL = 'test-rtt-serial'
TEST_GDB_PORT = 12345

# Expected subprocesses to be run for each command. Using the
# runner_config fixture (and always specifying gdb-serial) means we
# don't get 100% coverage, but it's a starting out point.
EXPECTED_COMMANDS = {
    'attach':
    ([RC_GDB,
      '-ex', "set confirm off",
      '-ex', "target extended-remote {}".format(TEST_GDB_SERIAL),
      '-ex', "monitor swdp_scan",
      '-ex', "attach 1",
      '-ex', "monitor rtt disable",
      '-ex', "file {}".format(RC_KERNEL_ELF)],),
    'debug':
    ([RC_GDB,
      '-ex', "set confirm off",
      '-ex', "target extended-remote {}".format(TEST_GDB_SERIAL),
      '-ex', "monitor swdp_scan",
      '-ex', "attach 1",
      '-ex', "monitor rtt disable",
      '-ex', "file {}".format(RC_KERNEL_ELF),
      '-ex', "load {}".format(RC_KERNEL_ELF)],),
    'flash':
    ([RC_GDB,
      '-ex', "set confirm off",
      '-ex', "set pagination off",
      '-ex', "target extended-remote {}".format(TEST_GDB_SERIAL),
      '-ex', "monitor swdp_scan",
      '-ex', "attach 1",
      '-ex', "monitor erase_mass",
      '-ex', "load {}".format(RC_KERNEL_ELF),
      '-ex', "kill",
      '-ex', "quit",
      '-silent'],),
}

EXPECTED_CONNECT_SRST_COMMAND = {
        'attach': 'monitor connect_rst disable',
        'debug': 'monitor connect_rst enable',
        'flash': 'monitor connect_rst enable',
}

def require_patch(program):
    assert program == RC_GDB

@pytest.mark.parametrize('command', EXPECTED_COMMANDS)
@patch('runners.core.ZephyrBinaryRunner.require', side_effect=require_patch)
@patch('runners.core.ZephyrBinaryRunner.check_call')
def test_blackmagicprobe_init(cc, _req, command, runner_config):
    '''Test commands using a runner created by constructor.'''
    runner = BlackMagicProbeRunner(runner_config, TEST_GDB_SERIAL)
    runner.run(command)
    assert cc.call_args_list == [call(x) for x in EXPECTED_COMMANDS[command]]

@pytest.mark.parametrize('command', EXPECTED_COMMANDS)
@patch('runners.core.ZephyrBinaryRunner.require', side_effect=require_patch)
@patch('runners.core.ZephyrBinaryRunner.check_call')
def test_blackmagicprobe_create(cc, _req, command, runner_config):
    '''Test commands using a runner created from command line parameters.'''
    args = ['--gdb-serial', TEST_GDB_SERIAL, '--erase']
    parser = argparse.ArgumentParser(allow_abbrev=False)
    BlackMagicProbeRunner.add_parser(parser)
    arg_namespace = parser.parse_args(args)
    runner = BlackMagicProbeRunner.create(runner_config, arg_namespace)
    runner.run(command)
    assert cc.call_args_list == [call(x) for x in EXPECTED_COMMANDS[command]]

@pytest.mark.parametrize('command', EXPECTED_CONNECT_SRST_COMMAND)
@patch('runners.core.ZephyrBinaryRunner.require', side_effect=require_patch)
@patch('runners.core.ZephyrBinaryRunner.check_call')
def test_blackmagicprobe_connect_rst(cc, _req, command, runner_config):
    '''Test that commands list the correct connect_rst value when enabled.'''
    args = ['--gdb-serial', TEST_GDB_SERIAL, '--erase', '--connect-rst']
    parser = argparse.ArgumentParser(allow_abbrev=False)
    BlackMagicProbeRunner.add_parser(parser)
    arg_namespace = parser.parse_args(args)
    runner = BlackMagicProbeRunner.create(runner_config, arg_namespace)
    runner.run(command)
    expected = EXPECTED_CONNECT_SRST_COMMAND[command]
    assert expected in cc.call_args_list[0][0][0]

@pytest.mark.parametrize('args, quiet', [
    (['--rtt-serial', TEST_RTT_SERIAL],             False),
    (['--rtt-serial', TEST_RTT_SERIAL, '--quiet'],  True)
])
@patch('runners.core.ZephyrBinaryRunner.require', side_effect=require_patch)
@patch('serial.serial_for_url')
@patch('runners.blackmagicprobe.Miniterm')
def test_blackmagicprobe_rtt(mt, sfu, _req, args, quiet, runner_config):
    parser = argparse.ArgumentParser(allow_abbrev=False)
    BlackMagicProbeRunner.add_parser(parser)
    arg_namespace = parser.parse_args(args)
    runner = BlackMagicProbeRunner.create(runner_config, arg_namespace)
    runner.logger = MagicMock()
    runner.run('rtt')
    sfu.assert_called_once_with(TEST_RTT_SERIAL, 115200, do_not_open=True)
    mt.assert_called_once_with(sfu(TEST_RTT_SERIAL, 115200, do_not_open=True))
    if quiet:
        runner.logger.info.assert_not_called()
    else:
        runner.logger.info.assert_called()
        assert 'RTT' in runner.logger.info.call_args.args[0]

@pytest.mark.parametrize('args, client_res, exp_raise, exp_port', [
    (['--gdb-serial', TEST_GDB_SERIAL], ('E01', ''), True, blackmagicprobe.DEFAULT_BMDP_NET_GDB_PORT),
    (['--gdb-serial', TEST_GDB_SERIAL], ('OK', ''), False, blackmagicprobe.DEFAULT_BMDP_NET_GDB_PORT),
    (['--gdb-serial', TEST_GDB_SERIAL, '--gdb-port', str(TEST_GDB_PORT)], ('OK', ''), False, TEST_GDB_PORT),
])
@patch('runners.core.ZephyrBinaryRunner.require', side_effect=require_patch)
@patch('runners.blackmagicprobe.RSPConnectorSerial')
@patch('runners.blackmagicprobe.GDBRSPClient')
@patch('runners.blackmagicprobe.RSPConnectorTCPServer')
@patch('runners.blackmagicprobe.RSPProxy')
def test_blackmagicprobe_debugserver(rsp_proxy, rsp_tcp_server, gdb_client, rsp_serial,
                            _req, args, client_res, exp_raise, exp_port, runner_config):
    gdb_client.return_value.execute.return_value = client_res

    parser = argparse.ArgumentParser(allow_abbrev=False)
    BlackMagicProbeRunner.add_parser(parser)
    arg_namespace = parser.parse_args(args)
    runner = BlackMagicProbeRunner.create(runner_config, arg_namespace)

    if exp_raise:
        with pytest.raises(RuntimeError):
            runner.run('debugserver')
    else:
        runner.run('debugserver')
        rsp_serial.assert_called_once_with(TEST_GDB_SERIAL, baudrate=115200)
        gdb_client.return_value.execute.assert_called()
        rsp_tcp_server.assert_called_once_with('', exp_port)

@pytest.mark.parametrize('dev_id, arg_gdb, arg_rtt, env_gdb, env_rtt, expected', [
        # from args
        (None, '/dev/ARG/GDB', '/dev/ARG/RTT', None, None, ('/dev/ARG/GDB', '/dev/ARG/RTT')),
        # from env
        (None, None, None, '/dev/ENV/GDB', '/dev/ENV/RTT', ('/dev/ENV/GDB', '/dev/ENV/RTT')),
        # GDB from dev-id
        ('COMX', None, '/dev/ARG/RTT', None, None, ('COMX', '/dev/ARG/RTT')),
        ('COMX', None, None, None, '/dev/ENV/RTT', ('COMX', '/dev/ENV/RTT')),
        ('/dev/ttyDEVIDGDB', None, '/dev/ARG/RTT', None, None, ('/dev/ttyDEVIDGDB', '/dev/ARG/RTT')),
        ('/dev/ttyDEVIDGDB', None, None, None, '/dev/ENV/RTT', ('/dev/ttyDEVIDGDB', '/dev/ENV/RTT')),
        ('/dev/cu.usbmodemDEVIDGDB', None, '/dev/ARG/RTT', None, None, ('/dev/cu.usbmodemDEVIDGDB', '/dev/ARG/RTT')),
        ('/dev/cu.usbmodemDEVIDGDB', None, None, None, '/dev/ENV/RTT', ('/dev/cu.usbmodemDEVIDGDB', '/dev/ENV/RTT')),
        ('COMDEVIDGDB', None, '/dev/ARG/RTT', None, None, ('COMDEVIDGDB', '/dev/ARG/RTT')),
        ('COMDEVIDGDB', None, None, None, '/dev/ENV/RTT', ('COMDEVIDGDB', '/dev/ENV/RTT')),
        # GDB from dev-id has to include tty or start with COM
        ('foobar', None, None, '/dev/ENV/GDB', '/dev/ENV/RTT', ('/dev/ENV/GDB', '/dev/ENV/RTT')),
        # args has priority over env
        (None, '/dev/ARG/GDB', '/dev/ARG/RTT', '/dev/ENV/GDB', '/dev/ENV/RTT', ('/dev/ARG/GDB', '/dev/ARG/RTT')),
        # args has priority over GDB from dev-id
        ('COM/dev/ttyDEVIDGDB', '/dev/ARG/GDB', '/dev/ARG/RTT', None, None, ('/dev/ARG/GDB', '/dev/ARG/RTT')),
        # GDB from dev-id has priority over env
        ('COM/dev/ttyDEVIDGDB', None, None, '/dev/ENV/GDB', '/dev/ENV/RTT', ('COM/dev/ttyDEVIDGDB', '/dev/ENV/RTT')),
        # ('COM/dev/ttyDEVIDGDB', '/dev/ARG/GDB', '/dev/ARG/RTT', '/dev/ENV/GDB', '/dev/ENV/RTT', ('EXPECTED', 'EXPECTED')),
    ])
def test_blackmagicprobe_serial_ports_generic(dev_id, arg_gdb, arg_rtt, env_gdb, env_rtt, expected):
    if 'BMP_GDB_SERIAL' in os.environ: os.environ.pop('BMP_GDB_SERIAL')
    if 'BMP_RTT_SERIAL' in os.environ: os.environ.pop('BMP_RTT_SERIAL')

    if env_gdb: os.environ['BMP_GDB_SERIAL'] = env_gdb
    if env_rtt: os.environ['BMP_RTT_SERIAL'] = env_rtt

    ret = blackmagicprobe.blackmagicprobe_ports(dev_id, arg_gdb, arg_rtt)
    assert expected == ret

def makeListPortInfo(device, description, vid, pid, serial_number, interface):
        # create ListPortInfo instance with link detection disabled
        port = serial.tools.list_ports_common.ListPortInfo(device, True)
        port.description = description or port.description
        port.vid = vid or port.vid
        port.pid = pid or port.pid
        port.serial_number = serial_number or port.serial_number
        port.interface = interface or port.interface
        return port

@pytest.mark.parametrize('device_id, ports, expected', [
        # no BMDP present
        (None, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
        ], (None, None)),
        # device has to be a BMDP
        ('BMP0000', [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # generic port with requested serial number
            ('/dev/tty1', None, None, None, 'BMP0000', None),       
        ], (None, None)),
        # identifies a BMDP GDB port
        (None, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
        ], ('/dev/ttyBmpGdb0', None)),
        # identifies BMDP GDB and RTT port
        (None, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
        ], ('/dev/ttyBmpGdb0', '/dev/ttyBmpRtt0')),
        # no device_id chooses in ascending order
        (None, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 1 GDB; SN: BMP0001
            ('/dev/ttyBmpGdb1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 1 RTT; SN: BMP0001
            ('/dev/ttyBmpRtt1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_RTT_INTERFACE),
        ], ('/dev/ttyBmpGdb0', '/dev/ttyBmpRtt0')),
        # identifies a BMDP GDB port by device id
        ('BMP0001', [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 1 GDB; SN: BMP0001
            ('/dev/ttyBmpGdb1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_GDB_INTERFACE),
        ], ('/dev/ttyBmpGdb1', None)),
        # identifies a BMDP GDB port by numerical device index (1-based)
        (2, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 1 GDB; SN: BMP0001
            ('/dev/ttyBmpGdb1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_GDB_INTERFACE),
        ], ('/dev/ttyBmpGdb1', None)),
        # identifies a BMDP GDB and RTT port by device id
        ('BMP0001', [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 1 GDB; SN: BMP0001
            ('/dev/ttyBmpGdb1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 1 RTT; SN: BMP0001
            ('/dev/ttyBmpRtt1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_RTT_INTERFACE),
        ], ('/dev/ttyBmpGdb1', '/dev/ttyBmpRtt1')),
        # identifies a BMDP GDB and RTT port by numerical device index (1-based)
        (2, [
            # generic port
            ('/dev/tty0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/ttyBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/ttyBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 1 GDB; SN: BMP0001
            ('/dev/ttyBmpGdb1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 1 RTT; SN: BMP0001
            ('/dev/ttyBmpRtt1', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0001', blackmagicprobe.BMP_RTT_INTERFACE),
        ], ('/dev/ttyBmpGdb1', '/dev/ttyBmpRtt1')),
        # identifies GDB and RTT interfaces independent of tty enumeration order
        (None, [
            # BMP 2 RTT; SN: BMP0002; generic device; wrong GDB/RTT port sort order
            ('/dev/tty21', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0002', blackmagicprobe.BMP_RTT_INTERFACE),
            # BMP 2 GDB; SN: BMP0002; generic device; wrong GDB/RTT port sort order
            ('/dev/tty22', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0002', blackmagicprobe.BMP_GDB_INTERFACE),
        ], ('/dev/tty22', '/dev/tty21')),
    ])
@patch('serial.tools.list_ports.comports')
def test_blackmagicprobe_gdb_serial_linux(stlpc, device_id, ports, expected):
    stlpc.return_value = [makeListPortInfo(*p) for p in ports]

    ret = blackmagicprobe.blackmagicprobe_ports_linux(device_id)
    assert expected == ret

@pytest.mark.parametrize('device_id, ports, expected', [
        # no BMDP present
        (None, [
            # generic port
            ('/dev/cu.usbmodem0', None, None, None, None, None),
        ], (None, None)),
        # guesses BMDP GDB port by description
        (None, [
            # generic port
            ('/dev/cu.usbmodem0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/cu.usbmodemBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
        ], ('/dev/cu.usbmodemBmpGdb0', None)),
        # guesses BMDP GDB and RTT port by description
        (None, [
            # generic port
            ('/dev/cu.usbmodem0', None, None, None, None, None),
            # BMP 0 GDB; SN: BMP0000
            ('/dev/cu.usbmodemBmpGdb0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_GDB_INTERFACE),
            # BMP 0 RTT; SN: BMP0000
            ('/dev/cu.usbmodemBmpRtt0', blackmagicprobe.BMP_PRODUCT,
                    blackmagicprobe.BMP_VID, blackmagicprobe.BMP_PID,
                    'BMP0000', blackmagicprobe.BMP_RTT_INTERFACE),
        ], ('/dev/cu.usbmodemBmpGdb0', '/dev/cu.usbmodemBmpRtt0')),
    ])
@patch('serial.tools.list_ports.comports')
def test_blackmagicprobe_gdb_serial_darwin(stlpc, device_id, ports, expected):
    stlpc.return_value = [makeListPortInfo(*p) for p in ports]

    ret = blackmagicprobe.blackmagicprobe_ports_darwin(device_id)
    assert expected == ret

# minimal mock pyusb backend; string enumerators hold strings
class MockUSBBackend(usb.backend.IBackend):
    class MockUSBDescriptorNamespace(SimpleNamespace):
        def __getattr__(self, name): return 0
    
    def __init__(self, devices): self.devs = devices

    def open_device(self, *_, **__): return {}
    def close_device(self, *_, **__): pass
    def enumerate_devices(self): yield from range(len(self.devs))
    def get_configuration(self, *_, **__): return 1

    def get_device_descriptor(self, h):
        d = self.devs[h]
        return MockUSBBackend.MockUSBDescriptorNamespace(
            idVendor=d["vid"],
            idProduct=d["pid"],
            iSerialNumber=d["serial"],
            bNumConfigurations=1,
        )

    def get_configuration_descriptor(self, h, *_, **__):
        return MockUSBBackend.MockUSBDescriptorNamespace(
            bNumInterfaces=len(self.devs[h].get("ifaces", [])),
            bConfigurationValue=1
        )

    def get_interface_descriptor(self, h, intf_idx, alt_idx, *_, **__):
        if alt_idx != 0: raise IndexError
        return MockUSBBackend.MockUSBDescriptorNamespace(
            bInterfaceNumber=intf_idx,
            iInterface=self.devs[h]["ifaces"][intf_idx]["desc_id"],
        )

def build_mock_registry_tree(devices):
    usb_subtree = {}
    for dev in devices:
        dev_enum = f"VID_{dev["vid"]:04X}&PID_{dev["pid"]:04X}"
        dev_subtree = { dev["serial"]: { "ParentIdPrefix": dev["parent_id_prefix"] } }
        if dev_enum in usb_subtree:
            usb_subtree[dev_enum] |= dev_subtree
        else:
            usb_subtree[dev_enum] = dev_subtree
            
        for iface_idx, iface in enumerate(dev['ifaces']):
            iface_enum = f"VID_{dev["vid"]:04X}&PID_{dev["pid"]:04X}&MI_{iface_idx:02X}"
            iface_subtree = { f"{dev["parent_id_prefix"]}&{iface_idx:04X}" : { "FriendlyName": iface["friendly_name"] } }
            if iface_enum in usb_subtree:
                usb_subtree[iface_enum] |= iface_subtree
            else:
                usb_subtree[iface_enum] = iface_subtree

    return { "SYSTEM": { "CurrentControlSet": { "Enum": { "USB": usb_subtree } } } }

def parametrize_devices_hklm(usb_devices):
    return usb_devices, build_mock_registry_tree(usb_devices)

@pytest.mark.parametrize('device_id, usb_devices, hklm, expected', [
        # no BMDP present
        (None, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
        ]), (None, None)),
        # device has to be a BMDP
        ('BMP0000', *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # generic port with requested serial number
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "BMP0000",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM2)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            }
        ]), (None, None)),
        # identifies a BMDP GDB port
        (None, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
        ]), ('COM10', None)),
        # identifies BMDP GDB and RTT port
        (None, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM11)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
        ]), ('COM10', 'COM11')),
        # no device_id chooses in ascending order of libusb backend enumeration
        (None, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM11)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
            # BMP 1; SN: BMP0001; reversed COM port enumeration order
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0001",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM21)',
                    },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM20)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM10', 'COM11')),
        # identifies a BMDP GDB port by device id
        ("BMP0001", *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
            # BMP 1; SN: BMP0001
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0001",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM20)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM20', None)),
        # identifies a BMDP GDB and RTT port by device id
        ("BMP0001", *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM11)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
            # BMP 1; SN: BMP0001
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0001",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM20)',
                    },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM21)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM20', 'COM21')),
        # identifies a BMDP GDB port by numerical device index (1-based)
        (2, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
            # BMP 1; SN: BMP0001
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0001",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM20)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM20', None)),
        # identifies a BMDP GDB and RTT port by numerical device index (1-based)
        (2, *parametrize_devices_hklm([
            # generic port
            {
                "vid": 0xBEEF,
                "pid": 0xDEAD,
                "serial": "ABC123",
                "ifaces": [
                {
                        "desc_id": 0x00,
                        "friendly_name": 'USB Serial Port (COM1)',
                    },
                ],
                "parent_id_prefix": "9&1234debe",
            },
            # BMP 0; SN: BMP0000
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0000",
                "ifaces": [
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM10)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM11)',
                    },
                    { "desc_id": 0xff, "friendly_name": 'DUMMY' },
                ],
                "parent_id_prefix": "0&2b3c4d5e",
            },
            # BMP 1; SN: BMP0001
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0001",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM20)',
                    },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM21)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM20', 'COM21')),
        # identifies GDB and RTT interfaces independent of COM port enumeration order
        (None, *parametrize_devices_hklm([
            # BMP 2 SN: BMP0002; reversed COM port enumeration order
            {
                "vid": blackmagicprobe.BMP_VID,
                "pid": blackmagicprobe.BMP_PID,
                "serial": "BMP0002",
                "ifaces": [
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_GDB,
                        "friendly_name": f'{blackmagicprobe.BMP_GDB_INTERFACE} (COM21)',
                    },
                    {
                        "desc_id": blackmagicprobe.BMP_IF_DESC_RTT,
                        "friendly_name": f'{blackmagicprobe.BMP_RTT_INTERFACE} (COM20)',
                    },
                ],
                "parent_id_prefix": "1&5e4d3c2b",
            },
        ]), ('COM21', 'COM20')),
    ])
@patch.dict(sys.modules, {
    "usb.backend.libusb1": MagicMock(),
    "libusb": MagicMock(),
    "libusb_package": MagicMock(),
    "win32con": MagicMock(),
    "win32api": MagicMock(),
})
def test_blackmagicprobe_gdb_serial_win32(device_id, usb_devices, hklm, expected):
    with patch.object(blackmagicprobe, "usb", usb, create=True), \
         patch.object(blackmagicprobe.usb.backend, "libusb1", create=True), \
         patch.object(blackmagicprobe, "libusb_package", create=True), \
         patch.object(blackmagicprobe, "win32api", create=True), \
         patch.object(blackmagicprobe, "win32con", create=True):

        blackmagicprobe.usb.util.get_string = lambda _dev, idx_as_value, _lid=None: idx_as_value
        blackmagicprobe.usb.backend.libusb1.get_backend = lambda *_, **__: MockUSBBackend(usb_devices)

        blackmagicprobe.win32con.HKEY_LOCAL_MACHINE = hklm
        blackmagicprobe.win32api.RegOpenKeyEx = lambda tree, path, *_, **__: reduce(lambda node, key: node[key], path.split('\\'), tree)
        blackmagicprobe.win32api.RegQueryValueEx = lambda tree, node: (tree[node], 1)

        ret = blackmagicprobe.blackmagicprobe_ports_win32(device_id)
        assert expected == ret


@pytest.mark.parametrize("payload, expected_chk", [
    # empty
    (b"", 0),
    # payload: 'O'(79) + 'K'(74)
    (b"OK", 154),
    # % 256 rollover
    (b"\xff\x01", 0),
])
def test_gdb_rsp_packet_checksum(payload, expected_chk):
    assert blackmagicprobe.RSPConnector.Packet._chk(payload) == expected_chk

@pytest.mark.parametrize("raw_input, expected_escaped", [
    # empty
    (b"", b""),
    # single byte
    (b"E", b"E"),
    # passthrough
    (b"foo:bar", b"foo:bar"),
    # escape single byte
    (b"#", b"}\x03"),
    # escape multiple and all required
    (b"#$%*}", b"}\x03}\x04}\x05}\x0a}\x5d"),
    # escape within unescaped data
    (b"foo#$bar%*foo}bar", b"foo}\x03}\x04bar}\x05}\x0afoo}\x5dbar"),
])
def test_gdb_rsp_packet_encode_escape(raw_input, expected_escaped):
    assert blackmagicprobe.RSPConnector.Packet._encode_escape(raw_input) == expected_escaped

@pytest.mark.parametrize("escaped_input, expected_decoded", [
    # empty
    (b"", b""),
    # single byte
    (b"E", b"E"),
    # passthrough
    (b"foo:bar", b"foo:bar"),
    # escape single byte
    (b"}\x03", b"#"),
    # escape multiple and all required
    (b"}\x03}\x04}\x05}\x0a}\x5d", b"#$%*}"),
    # escape within unescaped data
    (b"foo}\x03}\x04bar}\x05}\x0afoo}\x5dbar", b"foo#$bar%*foo}bar"),
    # RLE
    (b"a*<", b"a" * 32),
    # RLE escaped
    (b"}\x04* ", b"$$$$"),
    # RLE embedded
    (b"fo* bar", b"foooobar"),
])
def test_gdb_rsp_packet_decode_escape_rle(escaped_input, expected_decoded):
    assert blackmagicprobe.RSPConnector.Packet._decode_escape_rle(escaped_input) == expected_decoded

# _payload: raw escaped
# payload property: unescaped, RLE decoded
# printable: string, unescaped, RLE decoded, only printable characters
@pytest.mark.parametrize("cls, payload, exp_payload_prop, exp__payload, exp_printable_prop", [
    # empty packet is empty payload
    (blackmagicprobe.RSPConnector.Packet, None, b"", b"", ""),
    # empty payload is empty payload
    (blackmagicprobe.RSPConnector.Packet, b"", b"", b"", ""),
    # single byte payload
    (blackmagicprobe.RSPConnector.Packet, b"\x01", b"\x01", b"\x01", ""),
    # multi byte payload
    (blackmagicprobe.RSPConnector.Packet, b"OK", b"OK", b"OK", "OK"),
    # escapes
    (blackmagicprobe.RSPConnector.Packet, b"foo#$bar%*foo}bar", b"foo#$bar%*foo}bar", b"foo}\x03}\x04bar}\x05}\x0afoo}\x5dbar", "foo#$bar%*foo}bar"),
    # notification
    (blackmagicprobe.RSPConnector.Notification, b"Stop:T05", b"Stop:T05", b"Stop:T05", "% Stop:T05"),
    # control packets
    (blackmagicprobe.RSPConnector.ACK, None, b"+", b"+", "ACK"),
    (blackmagicprobe.RSPConnector.NACK, None, b"-", b"-", "NACK"),
    # signals
    (blackmagicprobe.RSPConnector.SIGINT, None, b"\x03", b"\x03", "SIGINT"),
    (blackmagicprobe.RSPConnector.EOT, None, b"\x04", b"\x04", "EOT"),
    (blackmagicprobe.RSPConnector.BREAK, None, b"\x1d", b"\x1d", "BREAK"),
])
def test_gdb_rsp_packet_instantiation_representation(cls, payload, exp_payload_prop, exp__payload, exp_printable_prop):
    pkt = cls(payload) if payload is not None else cls()
    assert pkt.payload == exp_payload_prop
    assert pkt._payload == exp__payload
    assert pkt.printable == exp_printable_prop

# _payload: raw escaped, RLE encoded
# payload property: unescaped, RLE decoded
# printable: string, unescaped, RLE decoded, only printable characters
@pytest.mark.parametrize("wire, exp_cls, exp_payload_prop, exp__payload, exp_printable_prop, exp_mon, exp_out", [
    # single byte payload
    (b"$\x01#01", blackmagicprobe.RSPConnector.Packet, b"\x01", b"\x01", "", False, False),
    # multi byte payload
    (b"$OK#9a", blackmagicprobe.RSPConnector.Packet, b"OK", b"OK", "OK", False, False),
    # escapes
    (b"$foo}\x03}\x04bar}\x05}\x0afoo}\x5dbar#d6", blackmagicprobe.RSPConnector.Packet, b"foo#$bar%*foo}bar", b"foo}\x03}\x04bar}\x05}\x0afoo}\x5dbar", "foo#$bar%*foo}bar", False, False),
    # RLE
    (b"$}\x04* #cb", blackmagicprobe.RSPConnector.Packet, b"$$$$", b"}\x04* ", "$$$$", False, False),
    # notification
    (b"%Stop:T05#99", blackmagicprobe.RSPConnector.Notification, b"Stop:T05", b"Stop:T05", "% Stop:T05", False, False),
    # control packets
    (b"+", blackmagicprobe.RSPConnector.ACK, b"+", b"+", "ACK", False, False),
    (b"-", blackmagicprobe.RSPConnector.NACK, b"-", b"-", "NACK", False, False),
    # signals
    (b"\x03", blackmagicprobe.RSPConnector.SIGINT, b"\x03", b"\x03", "SIGINT", False, False),
    (b"\x04", blackmagicprobe.RSPConnector.EOT, b"\x04", b"\x04", "EOT", False, False),
    (b"\x1d", blackmagicprobe.RSPConnector.BREAK, b"\x1d", b"\x1d", "BREAK", False, False),
    # output
    (b"$O666f6f626172#2b", blackmagicprobe.RSPConnector.Packet, b"O666f6f626172", b"O666f6f626172", "foobar", False, True),
    # monitor; no decoding sugar
    (b"$qRcmd,7265736574#37", blackmagicprobe.RSPConnector.Packet, b"qRcmd,7265736574", b"qRcmd,7265736574", "monitor reset", True, False),
])
def test_gdb_rsp_packet_decoding_representation(wire, exp_cls, exp_payload_prop, exp__payload, exp_printable_prop, exp_mon, exp_out):
    pkt = blackmagicprobe.RSPConnector.Packet.decode(wire)
    assert type(pkt) is exp_cls
    assert pkt.payload == exp_payload_prop
    assert pkt._payload == exp__payload
    assert pkt.printable == exp_printable_prop
    assert pkt.monitor == exp_mon
    assert pkt.output == exp_out

@pytest.mark.parametrize("payload, kwargs, exp_encoded, exp_mon, exp_out", [
    # 'O' does not make it output packet
    ("Ofoobar", {}, b"Ofoobar", False, False),
    # output
    ("foobar", {"output": True}, b"O666f6f626172", False, True),
    # explicit monitor
    ("reset", {"monitor": True}, b"qRcmd,7265736574", True, False),
    # monitor from payload
    ("monitor reset", {}, b"qRcmd,7265736574", True, False),
    # from bytes: no output
    (b"foobar", {"output": True}, b"foobar", False, False),
    # from bytes: no explicit monitor
    (b"reset", {"monitor": True}, b"reset", False, False),
    # from bytes: no monitor from payload
    (b"monitor reset", {}, b"monitor reset", False, False),
])
def test_gdb_rsp_packet_instantiation_string_sugar(payload, kwargs, exp_encoded, exp_mon, exp_out):
    pkt = blackmagicprobe.RSPConnector.Packet(payload, **kwargs)
    # # not testing checksum and framing
    # assert pkt.encoded[1:-3] == exp_encoded
    assert pkt.payload == exp_encoded
    assert pkt.monitor == exp_mon
    assert pkt.output == exp_out

@pytest.mark.parametrize("cls, payload, exp_encoded", [
    # empty packet is empty payload
    (blackmagicprobe.RSPConnector.Packet, None, b"$#00"),
    # empty payload is empty payload
    (blackmagicprobe.RSPConnector.Packet, b"", b"$#00"),
    # single byte payload
    (blackmagicprobe.RSPConnector.Packet, b"\x01", b"$\x01#01"),
    # multi byte payload
    (blackmagicprobe.RSPConnector.Packet, b"OK", b"$OK#9a"),
    # notification
    (blackmagicprobe.RSPConnector.Notification, b"Stop:T05", b"%Stop:T05#99"),
    # control packets
    (blackmagicprobe.RSPConnector.ACK, None, b"+"),
    (blackmagicprobe.RSPConnector.NACK, None, b"-"),
    # signals
    (blackmagicprobe.RSPConnector.SIGINT, None, b"\x03"),
    (blackmagicprobe.RSPConnector.EOT, None, b"\x04"),
    (blackmagicprobe.RSPConnector.BREAK, None, b"\x1d"),
])
def test_gdb_rsp_packet_encoded_wire_format(cls, payload, exp_encoded):
    pkt = cls(payload) if payload is not None else cls()
    assert pkt.encoded == exp_encoded

@pytest.mark.parametrize("obj1, obj2, exp_eq", [
    # Packet == Packet
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), blackmagicprobe.RSPConnector.Packet(b"$$$$"), True),
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), blackmagicprobe.RSPConnector.Packet(b"OK"), False),
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), b"$$$$", True),
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), b"}\x04* ", True),
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), b"OK", False),
    # Packet == str
    (blackmagicprobe.RSPConnector.Packet("$$$$"), "$$$$", True),
    (blackmagicprobe.RSPConnector.Packet("$$$$"), "OK", False),
    # Invalid comparison type
    (blackmagicprobe.RSPConnector.Packet(b"$$$$"), 0, False),
])
def test_gdb_rsp_packet_equality(obj1, obj2, exp_eq):
    assert (obj1 == obj2) == exp_eq

class MockTransportRSPConnector(blackmagicprobe.RSPConnector):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.read_buffer = bytearray()
        self.written_bytes = bytearray()

    def feed_rx(self, data: bytes):
        self.read_buffer.extend(data)

    def _read_impl(self, bufsize: int = 4096) -> bytes:
        chunk = bytes(self.read_buffer[:bufsize])
        self.read_buffer = self.read_buffer[bufsize:]
        return chunk

    def _write_impl(self, data: bytes):
        self.written_bytes.extend(data)

    def close(self): pass

@pytest.mark.parametrize("chunks, exp_payloads", [
    ([
        blackmagicprobe.RSPConnector.Packet("foobar", output=True).encoded,
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded
    ], [b"O666f6f626172", b"OK"]),
    ([
        blackmagicprobe.RSPConnector.Packet("foobar", output=True).encoded +
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded
    ], [b"O666f6f626172", b"OK"]),
    ([
        b"noise" +
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded
    ], [b"OK"]),
    ([
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded +
        b"noise" +
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded
    ], [b"OK", b"OK"]),
    # SIGINT from within junk
    ([b"foo\x03bar"], [b"\x03"]),
    # lost sync with escaped # does not read as SIGINT
    ([b"}\x03"], []),
    # ACK not handled from queue
    ([
        blackmagicprobe.RSPConnector.ACK().encoded +
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded
    ], [b"OK"]),
])
def test_gdb_rsp_connector_rx_stream_packet_decoding(chunks, exp_payloads):
    conn = MockTransportRSPConnector()
    conn.no_ack = True
    
    for chunk in chunks:
        conn.feed_rx(chunk)
        conn.spin()

    received = [pkt.payload for pkt in conn.rxq][::-1]
    assert received == exp_payloads

@pytest.mark.parametrize("rx_stream, exp_tx, exp_rx_count", [
    # valid --> ACK
    (
        blackmagicprobe.RSPConnector.Packet(b"OK").encoded,
        blackmagicprobe.RSPConnector.ACK().encoded, 1
    ),
    # invalid --> NACK
    (
        b"$OK#00",
        blackmagicprobe.RSPConnector.NACK().encoded, 0
    ),
    # only standard packets use ACK/NACK
    (
        blackmagicprobe.RSPConnector.Notification(b"Stop:T05#a8").encoded,
        b"", 1
    ),
    (
        blackmagicprobe.RSPConnector.SIGINT().encoded,
        b"", 1
    ),
])
def test_gdb_rsp_connector_rx_ack_nack_generation(rx_stream, exp_tx, exp_rx_count):
    conn = MockTransportRSPConnector()
    conn.feed_rx(rx_stream)
    conn.spin()

    assert bytes(conn.written_bytes) == exp_tx
    assert len(conn.rxq) == exp_rx_count

def test_gdb_rsp_connector_tx_ack_nack_retransmit():
    conn = MockTransportRSPConnector()

    # send packet; awaiting ACK
    pkt = blackmagicprobe.RSPConnector.Packet(b"OK")
    conn.txq.append(pkt)
    conn._send_packets()
    assert bytes(conn.written_bytes) == b"$OK#9a"
    assert conn._send_await_ack is True
    conn.written_bytes.clear()

    # recv NACK; retransmit
    conn.feed_rx(blackmagicprobe.RSPConnector.NACK().encoded)
    conn._recv_packets()
    conn._send_packets()
    assert bytes(conn.written_bytes) == b"$OK#9a"
    assert conn._send_await_ack is True
    conn.written_bytes.clear()

    # revc ACK; end
    conn.feed_rx(blackmagicprobe.RSPConnector.ACK().encoded)
    conn._recv_packets()
    conn._send_packets()
    assert bytes(conn.written_bytes) == b""
    assert conn._send_await_ack is False

def test_gdb_rsp_connector_await_ack_timeout():
    conn = MockTransportRSPConnector(timeout=0.05)
    conn.txq.append(blackmagicprobe.RSPConnector.Packet("OK"))
    with pytest.raises(TimeoutError):
        conn.spin()

def test_gdb_rsp_connector_await_rx_timeout():
    conn = MockTransportRSPConnector(timeout=0.05)
    conn.feed_rx(b"$OK")
    with pytest.raises(TimeoutError):
        conn.spin()

class MockConnector():
    def __init__(self):
        self.rxq = deque()
        self.txq = deque()
        self._responses = []

    def feed_responses(self, responses):
        self._responses += responses

    def spin(self):
        self.rxq = deque(self._responses[::-1])
        self._responses = []

@pytest.mark.parametrize("mgo, cmd, res_pkgs, exp_res, exp_out, exp_call_oh, exp_call_nh", [
    # command response
    (
        True, "vDummyCommand",
        [
            blackmagicprobe.RSPConnector.Packet(b"OK")
        ],
        blackmagicprobe.RSPConnector.Packet(b"OK"), "",
        [], []
    ),
    # output handler called
    (
        True, "vDummyCommand",
        [
            blackmagicprobe.RSPConnector.Packet("output", output=True),
            blackmagicprobe.RSPConnector.Packet(b"OK")
        ],
        blackmagicprobe.RSPConnector.Packet(b"OK"), "",
        [blackmagicprobe.RSPConnector.Packet("output", output=True)], []
    ),
    # notification handler called
    (
        True, "vDummyCommand",
        [
            blackmagicprobe.RSPConnector.Notification(b"Stop"),
            blackmagicprobe.RSPConnector.Packet(b"OK")
        ],
        blackmagicprobe.RSPConnector.Packet(b"OK"), "",
        [], [blackmagicprobe.RSPConnector.Notification(b"Stop")]
    ),
    # monitor command gathers output as return if monitor_gater_output=True
    (
        True, "monitor",
        [
            blackmagicprobe.RSPConnector.Packet("output", output=True),
            blackmagicprobe.RSPConnector.Packet(b"OK")
        ],
        blackmagicprobe.RSPConnector.Packet("OK"), "output",
        [], []
    ),
    # monitor command does not gather output as return if monitor_gater_output=False
    (
        False, "monitor",
        [
            blackmagicprobe.RSPConnector.Packet("output", output=True),
            blackmagicprobe.RSPConnector.Packet(b"OK")
        ],
        blackmagicprobe.RSPConnector.Packet("OK"), "",
        [blackmagicprobe.RSPConnector.Packet("output", output=True)], []
    ),
])
def test_gdb_rsp_client_transactions(mgo, cmd, res_pkgs, exp_res, exp_out, exp_call_oh, exp_call_nh):
    output_handler=MagicMock()
    notification_handler=MagicMock()
    connector = MockConnector()
    client = blackmagicprobe.GDBRSPClient(connector,
                                          monitor_gater_output=mgo,
                                          output_handler=output_handler,
                                          notification_handler=notification_handler)

    connector.feed_responses(res_pkgs)
    res, out = client.execute(cmd)

    assert res == exp_res
    assert out == exp_out
    if len(exp_call_oh):
        for call in exp_call_oh:
            output_handler.assert_called_with(call)
    else:
        output_handler.assert_not_called()
    if len(exp_call_nh):
        for call in exp_call_nh:
            notification_handler.assert_called_with(call)
    else:
        notification_handler.assert_not_called()


@pytest.mark.parametrize("server_pkts, client_pkts", [
    # single packet each
    (
        [ blackmagicprobe.RSPConnector.Packet(b"SERVER") ],
        [ blackmagicprobe.RSPConnector.Packet(b"CLIENT") ]
    ),
    # multiple; unmatched
    (
        [
            blackmagicprobe.RSPConnector.Packet(b"SERVER0"),
            blackmagicprobe.RSPConnector.Packet(b"SERVER1"),
            blackmagicprobe.RSPConnector.Packet(b"SERVER2")
        ],
        [
            blackmagicprobe.RSPConnector.Packet(b"CLIENT0"),
            blackmagicprobe.RSPConnector.Packet(b"CLIENT1")
        ]
    ),
])
def test_gdb_rsp_proxy_packet_forwarding(server_pkts, client_pkts):
    server = MockConnector()
    client = MockConnector()
    proxy = blackmagicprobe.RSPProxy(server, client)

    server.feed_responses(server_pkts)
    client.feed_responses(client_pkts)
    
    while True:
        proxy._spin_once()
        if len(server.rxq) == 0 and len(client.rxq) == 0:
            break

    assert list(client.txq) == server_pkts
    assert list(server.txq) == client_pkts

def test_gdb_rsp_proxy_middleware_forward_reply():
    class TestMiddleware(blackmagicprobe.RSPProxy.ProxyMiddleware):
        def __init__(self, token):
            super().__init__()
            self.token = token

        def reply(self, packet, ctx):
            ctx.reply(blackmagicprobe.RSPConnector.Packet(packet.payload + self.token))

    server = MockConnector()
    client = MockConnector()
    proxy = blackmagicprobe.RSPProxy(server, client)
    proxy.middlewares = [TestMiddleware(b"MW0"), TestMiddleware(b"MW1")]

    server.feed_responses([blackmagicprobe.RSPConnector.Packet(b"SERVER")])
    client.feed_responses([blackmagicprobe.RSPConnector.Packet(b"CLIENT")])
    
    while True:
        proxy._spin_once()
        if len(server.rxq) == 0 and len(client.rxq) == 0:
            break

    assert [p.payload for p in list(server.txq)] == [b"CLIENT"]
    # reverse order for reply from server
    assert [p.payload for p in list(client.txq)] == [b"SERVERMW1MW0"]

def test_gdb_rsp_proxy_middleware_forward_request():
    class TestMiddleware(blackmagicprobe.RSPProxy.ProxyMiddleware):
        def __init__(self, token):
            super().__init__()
            self.token = token

        def request(self, packet, ctx):
            ctx.request(blackmagicprobe.RSPConnector.Packet(packet.payload + self.token))

    server = MockConnector()
    client = MockConnector()
    proxy = blackmagicprobe.RSPProxy(server, client)
    proxy.middlewares = [TestMiddleware(b"MW0"), TestMiddleware(b"MW1")]

    server.feed_responses([blackmagicprobe.RSPConnector.Packet(b"SERVER")])
    client.feed_responses([blackmagicprobe.RSPConnector.Packet(b"CLIENT")])
    
    while True:
        proxy._spin_once()
        if len(server.rxq) == 0 and len(client.rxq) == 0:
            break

    assert [p.payload for p in list(server.txq)] == [b"CLIENTMW0MW1"]
    assert [p.payload for p in list(client.txq)] == [b"SERVER"]

def test_gdb_rsp_proxy_middleware_short_circuit():
    class ShortCircuitMiddleware(blackmagicprobe.RSPProxy.ProxyMiddleware):
        def request(self, packet, ctx): ctx.reply(packet)
        def reply(self, packet, ctx): ctx.request(packet)

    server = MockConnector()
    client = MockConnector()
    proxy = blackmagicprobe.RSPProxy(server, client)

    mw_cl_pass = blackmagicprobe.RSPProxy.ProxyMiddleware()
    mw_cl_sc = ShortCircuitMiddleware()
    mw_none = blackmagicprobe.RSPProxy.ProxyMiddleware()
    mw_ss_sc = ShortCircuitMiddleware()

    proxy.middlewares = [mw_cl_pass, mw_cl_sc, mw_none, mw_ss_sc]

    with    patch.object(mw_cl_pass, 'request', wraps=mw_cl_pass.request) as mw_cl_pass_req, \
            patch.object(mw_cl_pass, 'reply', wraps=mw_cl_pass.reply) as mw_cl_pass_rep, \
            patch.object(mw_cl_sc, 'request', wraps=mw_cl_sc.request) as mw_cl_sc_req, \
            patch.object(mw_cl_sc, 'reply', wraps=mw_cl_sc.reply) as mw_cl_sc_rep, \
            patch.object(mw_none, 'request', wraps=mw_none.request) as mw_none_req, \
            patch.object(mw_none, 'reply', wraps=mw_none.reply) as mw_none_rep, \
            patch.object(mw_ss_sc, 'request', wraps=mw_ss_sc.request) as mw_ss_sc_req, \
            patch.object(mw_ss_sc, 'reply', wraps=mw_ss_sc.reply) as mw_ss_sc_rep:
 
        server.feed_responses([blackmagicprobe.RSPConnector.Packet(b"SERVER")])
        client.feed_responses([blackmagicprobe.RSPConnector.Packet(b"CLIENT")])
        
        while True:
            proxy._spin_once()
            if len(server.rxq) == 0 and len(client.rxq) == 0:
                break

        assert [p.payload for p in list(server.txq)] == [b"SERVER"]
        assert [p.payload for p in list(client.txq)] == [b"CLIENT"]
        assert mw_cl_pass_req.call_count == 1
        assert mw_cl_pass_rep.call_count == 1
        assert mw_cl_sc_req.call_count == 1
        assert mw_cl_sc_rep.call_count == 0
        assert mw_none_req.call_count == 0
        assert mw_none_rep.call_count == 0
        assert mw_ss_sc_req.call_count == 0
        assert mw_ss_sc_rep.call_count == 1
