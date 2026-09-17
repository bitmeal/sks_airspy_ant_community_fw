# Copyright (c) 2018 Roman Tataurov <diytronic@yandex.ru>
# Modified 2018 Tavish Naruka <tavishnaruka@gmail.com>
# Copyright (c) 2026 Arne Wendt <arne.wendt@tuhh.de>
#
# SPDX-License-Identifier: Apache-2.0
'''Runner for flashing with Black Magic Probe.'''
# https://github.com/blacksphere/blackmagic/wiki

import os
import signal
import sys
import time
import re
from collections import deque
from functools import reduce
import socket
from pathlib import Path
from abc import ABC, abstractmethod
import logging

from runners.core import RunnerCaps, ZephyrBinaryRunner

try:
    import serial
    from serial.tools.miniterm import Miniterm

    if sys.platform.startswith('linux') or sys.platform.startswith('darwin'):
        import serial.tools.list_ports

    if sys.platform.startswith('win32'):
        import usb.core
        import usb.backend.libusb1
        import libusb
        import libusb_package
        import win32api
        import win32con

    MISSING_REQUIREMENTS = False
except ImportError:
    MISSING_REQUIREMENTS = True


# Interface descriptor for the GDB port as defined in the BMP firmware.
BMP_GDB_INTERFACE = 'Black Magic GDB Server'
BMP_RTT_INTERFACE = 'Black Magic UART Port'

# Product string as defined in the BMP firmware.
BMP_PRODUCT = "Black Magic Probe"

# BMP vendor and product ID.
BMP_VID = 0x1d50
BMP_PID = 0x6018
BMP_IF_DESC_GDB = 0x4
BMP_IF_DESC_RTT = 0x5

# default configuration matching openOCD
DEFAULT_BMDP_NET_GDB_PORT = 3333

def blackmagicprobe_ports_linux(device_id):
    '''Find GDB and UART/RTT port for specified (or first) probe'''
    resolved_device_id = None
    
    ports = [port for port in serial.tools.list_ports.comports() if port.interface == BMP_GDB_INTERFACE]

    if device_id is not None:
        device_id_int = None
        try:
            device_id_int = int(device_id) - 1
        except Exception:
            pass

        dev_id_sn_ports = [port for port in ports if str(port.serial_number) == str(device_id)]
        # choose by serial number
        if len(dev_id_sn_ports):
            resolved_device_id = dev_id_sn_ports[0].serial_number
        # choose by id; as found/enumerated by pyserial
        elif device_id_int is not None and device_id_int < len(ports):
            resolved_device_id = ports[device_id_int].serial_number
    # choose first one
    elif len(ports):
        resolved_device_id = ports[0].serial_number
    
    if resolved_device_id is not None:
        gdb_port = ([port.device for port in serial.tools.list_ports.comports() if port.serial_number == resolved_device_id and port.interface == BMP_GDB_INTERFACE][0:1] or [None])[0]
        rtt_port = ([port.device for port in serial.tools.list_ports.comports() if port.serial_number == resolved_device_id and port.interface == BMP_RTT_INTERFACE][0:1] or [None])[0]
        return gdb_port, rtt_port
    else:
        return None, None

# no device-id support
def blackmagicprobe_ports_darwin(_device_id):
    '''Guess the GDB and UART/RTT port on Darwin platforms.'''
    bmp_ports = []
    for port in serial.tools.list_ports.comports():
        if port.description and port.description.startswith(BMP_PRODUCT):
            bmp_ports.append(port.device)
    if bmp_ports:
        return (*(sorted(bmp_ports)[:2] + [None, None])[:2],)
    else:
        return None, None

def blackmagicprobe_ports_win32(device_id):
    '''Find GDB and UART/RTT port for specified (or first) probe'''
    def zephyr_find_libusb_win(lib_name):
        dll_path = os.path.join(
            os.path.dirname(libusb.__file__),
            "_platform",
            "_windows",
            'x64' if sys.maxsize > 2**32 else 'x86',
            "libusb-1.0.dll")

        if "usb" in lib_name and os.path.exists(dll_path):
            return dll_path
        return None

    backend = usb.backend.libusb1.get_backend(find_library=libusb_package.find_library)
    if not backend:
        backend = usb.backend.libusb1.get_backend(find_library=zephyr_find_libusb_win)
    devices = list(usb.core.find(idVendor=BMP_VID, idProduct=BMP_PID, backend=backend, find_all=True))

    # read COM port from registry
    def get_dev_iface_com_port(device, interface):
        hklm = win32con.HKEY_LOCAL_MACHINE
        ccs_enum_usb = r"SYSTEM\CurrentControlSet\Enum\USB"
        reg_usb_enum = f"VID_{BMP_VID:04X}&PID_{BMP_PID:04X}"
        ifaces = list(usb.util.find_descriptor(device.get_active_configuration(), find_all=True, iInterface=interface))

        device_reg_key = win32api.RegOpenKeyEx(hklm, f"{ccs_enum_usb}\\{reg_usb_enum}\\{device.serial_number}", 0, win32con.KEY_READ)
        dev_parent_prefix, _ = win32api.RegQueryValueEx(device_reg_key, "ParentIdPrefix")
        win32api.RegCloseKey(device_reg_key)

        # choose first GDB interface if any exists
        if len(ifaces):
            reg_usb_gdb_enum = f"VID_{BMP_VID:04X}&PID_{BMP_PID:04X}&MI_{ifaces[0].bInterfaceNumber:02X}\\{dev_parent_prefix}&{ifaces[0].bInterfaceNumber:04X}"
            
            iface_reg_key = win32api.RegOpenKeyEx(hklm, f"{ccs_enum_usb}\\{reg_usb_gdb_enum}", 0, win32con.KEY_READ)
            com_port, _ = win32api.RegQueryValueEx(iface_reg_key, "FriendlyName")
            win32api.RegCloseKey(iface_reg_key)

            return re.search(r'COM\d+', com_port).group(0)

    device = None
    if device_id is not None:
        device_id_int = None
        try:
            device_id_int = int(device_id) - 1
        except Exception:
            pass

        dev_id_sn_devices = [dev for dev in devices if str(dev.serial_number) == str(device_id)]
        if len(dev_id_sn_devices):
            device = dev_id_sn_devices[0]
        # choose by id; as found/enumerated by pyserial
        elif device_id_int is not None and device_id_int < len(devices):
            device = devices[device_id_int]
    # choose first one
    elif len(devices):
        device = devices[0]
    
    if device is not None:
        gdb_port = get_dev_iface_com_port(device, BMP_IF_DESC_GDB)
        rtt_port = get_dev_iface_com_port(device, BMP_IF_DESC_RTT)
        return gdb_port, rtt_port
    else:
        return None, None

def blackmagicprobe_ports(device_id=None, gdb_serial=None, rtt_serial=None):
    '''Find GDB and RTT/UART port for the probe

    Return the ports to use, in order of priority:
        - the port specified manually
        - the port in the BMP_GDB_SERIAL, BMP_RTT_SERIAL environment variables
        - by specified device ID
        - the first ones matching BMDP descriptors
    '''
    # device id is serial port heuristic
    if device_id and (device_id.startswith("COM") or device_id.startswith("/dev/tty") or device_id.startswith("/dev/cu.usbmodem")) and gdb_serial is None:
        gdb_serial = device_id
        device_id = None

    gdb_port = gdb_serial if gdb_serial is not None else os.environ['BMP_GDB_SERIAL'] if 'BMP_GDB_SERIAL' in os.environ else None
    rtt_port = rtt_serial if rtt_serial is not None else os.environ['BMP_RTT_SERIAL'] if 'BMP_RTT_SERIAL' in os.environ else None
    
    if None in [gdb_port, rtt_port]:
        if sys.platform.startswith('linux'):
            ports = blackmagicprobe_ports_linux(device_id)
        elif sys.platform.startswith('darwin'):
            ports = blackmagicprobe_ports_darwin(device_id)
        elif sys.platform.startswith('win32'):
            ports = blackmagicprobe_ports_win32(device_id)
        else:
            raise RuntimeError(f'unsupported platform: {sys.platform}')
        
        gdb_port = gdb_port or (ports[0:1] or [None])[0]
        rtt_port = rtt_port or (ports[1:2] or [None])[0]

    return gdb_port, rtt_port


class RSPConnector(ABC):
    class Packet:
        START = b'$'

        def __eq__(self,other):
            if isinstance(other, RSPConnector.Packet)   : return self.payload == other.payload
            elif isinstance(other, bytes)               : return self.payload == other or self.payload == self._decode_escape_rle(other)
            elif isinstance(other, str)                 : return self.printable == other
            return False

        def __init__(self, payload = b'', **kwargs):
            self._monitor = self._output = False
            if isinstance(payload, bytes):
                self._payload = self._encode_escape(payload)
            elif isinstance(payload, str):
                if kwargs.get('output', False):
                    self._output = True
                    self._payload = b'O' + payload.encode('utf-8').hex().encode('ascii')
                elif kwargs.get('monitor', False) or payload.startswith('monitor'):
                    self._monitor = True
                    qRcmd = re.sub(r'^monitor\s*', '', payload.strip())
                    self._payload = f"qRcmd,{qRcmd.encode('ascii').hex()}".encode('ascii')
                    self._monitor = True
                else:
                    self._payload = self._encode_escape(payload.encode('ascii'))
            else:
                raise ValueError("Payload must be str or bytes")

        @staticmethod
        def _encode_escape(payload):
            def escapeEncoder(acc, val):
                _val = bytes([val])
                if _val in b'$%#}*' : return acc + b'}' + bytes([val^0x20])
                else                : return acc + _val
            return reduce(escapeEncoder, payload, b'')

        @staticmethod
        def _decode_escape_rle(payload):
            def escapeRLEDecoder(acc, val):
                decoded, escape, rle = acc
                if val == b'}'[0]   : return (decoded, True, False)
                elif val == b'*'[0] : return (decoded, False, True)
                elif escape         : return (decoded + bytes([val^0x20]), False, False)
                elif rle            : return (decoded + decoded[-1:]*(val - 29), False, False)
                else                : return (decoded + bytes([val]), False, False)
            return reduce(escapeRLEDecoder, payload, (b'', False, False))[0]

        @property
        def output(self): return self._output
        @property
        def monitor(self): return self._monitor

        @property
        # usable payload getter: unescaped and RLE decoded
        def payload(self):
            return self._decode_escape_rle(self._payload)

        @property
        def encoded(self): return self.START + self._payload + b'#' + f'{self._chk(self._payload):02x}'.encode('ascii')

        @property
        def printable(self):
            if self.output      : return bytes.fromhex(self.payload[1:].decode('ascii')).decode('utf-8')
            elif self.monitor   : return 'monitor ' + bytes.fromhex(self.payload[6:].decode('ascii')).decode('ascii')
            else                : return ''.join([c for c in self.payload.decode('utf-8', errors='ignore') if c.isprintable()])

        @classmethod
        def decode(cls, data):
            match data:
                case RSPConnector.ACK.START   : return RSPConnector.ACK()
                case RSPConnector.NACK.START  : return RSPConnector.NACK()
                case RSPConnector.SIGINT.START: return RSPConnector.SIGINT()
                case RSPConnector.EOT.START   : return RSPConnector.EOT()
                case RSPConnector.BREAK.START : return RSPConnector.BREAK()
            if (data[0:1] != cls.START and data[0:1] != RSPConnector.Notification.START) or data[-3:-2] != b'#':
                raise ValueError
            payload = data[1:-3]
            if int(data[-2:], 16) != cls._chk(payload):
                raise RuntimeError('-> CHK mismatch')
            
            output, monitor = False, False
            if payload.startswith(b'O') and payload != b'OK':
                try:
                    bytes.fromhex(payload[1:].decode('ascii'))
                    output = True
                except ValueError:
                    pass
            elif payload.startswith(b'qRcmd,'):
                monitor = True

            pkt = (RSPConnector.Notification if data[0:1] == RSPConnector.Notification.START else cls)()
            pkt._payload = payload
            pkt._output = output
            pkt._monitor = monitor
            return pkt
        
        @classmethod
        def _chk(cls, payload): return sum(payload) % 256

    class Notification(Packet):
        START = b'%'

        @property
        def printable(self): return '% ' + super().printable

    class SingleBytePacket(Packet):
        def __init__(self):
            self._output = False
            self._monitor = False

        @property
        def encoded(self): return self._payload

        @property
        def printable(self): return self.__class__.__name__

    class ACK(SingleBytePacket)     : START = _payload = b'+'
    class NACK(SingleBytePacket)    : START = _payload = b'-'
    class SIGINT(SingleBytePacket)  : START = _payload = b'\x03'
    class EOT(SingleBytePacket)     : START = _payload = b'\x04'
    class BREAK(SingleBytePacket)   : START = _payload = b'\x1d'
    
    def __init__(self, **kwargs):
        self.logger = logging.getLogger(f'runners.{BlackMagicProbeRunner.name()}.{self.__class__.__name__}')

        self.timeout = kwargs.get('timeout', 1.0)
        self.no_ack = kwargs.get('no_ack', False)
        # using: L->R; use .pop() to retrieve in order
        self.rxq = deque()
        # using: R->L; use .append()
        self.txq = deque()
        self._send_await_ack = False
        self._ack_start_time = None

    # reads all present packets on the wire
    # sends ACK/NACK and handles incoming ACK/NACK
    def _recv_packets(self):
        recv_buff = self._recv()
        # search first valid byte
        while recv_buff:
            match recv_buff[0:1]:
                case RSPConnector.ACK.START:
                    if self._send_await_ack:
                        self.txq.popleft()
                        self._send_await_ack = False
                        self._ack_start_time = None
                    self.logger.debug(f'-> [{RSPConnector.ACK.printable}]')
                case RSPConnector.NACK.START:
                    self._send_await_ack = False
                    self._ack_start_time = None
                    self.logger.debug(f'-> [{RSPConnector.NACK.printable}]')
                case RSPConnector.SIGINT.START | RSPConnector.EOT.START | RSPConnector.BREAK.START:
                    self.rxq.appendleft(RSPConnector.Packet.decode(recv_buff[0:1]))
                    self.logger.debug(f'-> [{self.rxq[0].printable}]')
                case RSPConnector.Packet.START | RSPConnector.Notification.START:
                    end = recv_buff.find(b'#')
                    start_time = time.monotonic()
                    while end == -1 or len(recv_buff) < end + 3:
                        recv_buff += self._recv()
                        end = recv_buff.find(b'#')
                        if time.monotonic() - start_time > self.timeout:
                            raise TimeoutError(f"Timed out waiting for full RSP frame!")
                    try:
                        self.rxq.appendleft(RSPConnector.Packet.decode(recv_buff[0:end+3]))
                        self.logger.debug(f'-> [{self.rxq[0].printable}]')
                        if recv_buff[0:1] == RSPConnector.Packet.START and not self.no_ack:
                            self._send(RSPConnector.ACK())
                            self.logger.debug(f'<- [{RSPConnector.ACK().printable}]')
                    except:
                        self.logger.warning(f'failed decoding packet, with: {print(repr(sys.exception()))}')
                        if recv_buff[0:1] == RSPConnector.Packet.START and not self.no_ack:
                            self._send(RSPConnector.NACK())
                            self.logger.debug(f'<- [{RSPConnector.NACK().printable}]')
                    recv_buff = recv_buff[end+3:]
                    continue
                case b'}': # lost somewhere with an escaped '#' (0x23^0x20)
                    if len(recv_buff) == 1:
                        recv_buff += self._recv()
                    elif recv_buff[1:2] == b'\x03':
                        recv_buff = recv_buff[2:]
                    continue
                case _ :
                    self.logger.debug('read illegal character')
            recv_buff = recv_buff[1:]
            recv_buff += self._recv()

    # sends all queued packets, until ACK has to be awaited
    def _send_packets(self):
        if self._send_await_ack:
            if time.monotonic() - self._ack_start_time > self.timeout:
                self._send_await_ack = False
                self._ack_start_time = None
                raise TimeoutError("Timed out waiting for RSP ACK response from target.")
        else:
            while len(self.txq):
                pkg = self.txq[0]
                self._send(pkg)
                self.logger.debug(f'<- [{pkg.printable}]')
                if type(pkg) == self.Packet and not self.no_ack:
                    self._send_await_ack = True
                    self._ack_start_time = time.monotonic()
                    break
                else:
                    self.txq.popleft()

    def spin(self):
        while True:
            self._recv_packets()
            self._send_packets()
            if not self._send_await_ack:
                break            
    
    def _recv(self, bufsize=4096): return self._read_impl(bufsize)
    @abstractmethod
    def _read_impl(self, bufsize): pass
    
    def _send(self, packet): return self._write_impl(packet.encoded)
    @abstractmethod
    def _write_impl(self, data): pass
    
    @abstractmethod
    def close(self): pass


class GDBRSPClient():
    def __init__(self, connection, **kwargs):
        self.logger = logging.getLogger(f'runners.{BlackMagicProbeRunner.name()}.{self.__class__.__name__}')

        self.connection = connection

        self.monitor_gather_output = kwargs.get('monitor_gater_output', True)
        self.output_handler = kwargs.get('output_handler', lambda *_,**__:None)
        self.notification_handler = kwargs.get('notification_handler', lambda *_,**__:None)

    def transact(self, req):
        if len(self.connection.rxq):
            self.connection.rxq = deque()
            self.logger.warning('dropped packets from RSP input buffer')

        self.connection.txq.append(req)
        res = []
        while not any([(type(p) is RSPConnector.Packet and not p.output) for p in res]):
            self.connection.spin()
            while len(self.connection.rxq):
                pkg = self.connection.rxq.pop()
                if type(pkg) is RSPConnector.Notification:
                    self.notification_handler(pkg)
                elif type(pkg) is RSPConnector.Packet:
                    if pkg.output and not (req.monitor and self.monitor_gather_output):
                        self.output_handler(pkg)
                    else:
                        res.append(pkg)
                else:
                    self.logger.warning(f'unexpected RSP packet: {pkg.printable}')
        return res

    def execute_raw(self, command):
        req = RSPConnector.Packet(command)
        self.logger.debug(f"<- {command}")
        return self.transact(req)
    
    def execute(self, command):
        res = self.execute_raw(command)
        output = ''.join([r.printable for r in res if r.output])
        result = b''.join([r.payload for r in res if not r.output])
        return result, output


class RSPConnectorTCPClient(RSPConnector):
    def __init__(self, host, port, **kwargs):
        super().__init__(**kwargs)
        self.conn_timeout = kwargs.get('conn_timeout', 0.0001)
        self.logger.info(f"Connecting to {host}:{port}")
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # self.sock.settimeout(3.0)
        self.sock.connect((host, port))
        self.logger.info(f"connected")
        self._configure_socket()

    def _configure_socket(self):
        try:
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 1)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        except AttributeError:
            pass
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(self.conn_timeout)

    def _read_impl(self, bufsize=4096):
        try:
            data = self.sock.recv(bufsize)
            if bufsize and not data:
                self.logger.error("TCP connection closed by peer.")
                raise ConnectionResetError("Target disconnected")
            return data
        except socket.timeout:
            return b''
    
    def _write_impl(self, data):
        self.sock.sendall(data)

    def close(self):
        self.sock.close()

class RSPConnectorTCPServer(RSPConnectorTCPClient):
    def __init__(self, host, port, **kwargs):
        RSPConnector.__init__(self, **kwargs)
        self.conn_timeout = kwargs.get('conn_timeout', 0.0001)
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind((host or '', port))
        server_sock.listen(1)
        self.logger.info(f"Listening on {host or ''}:{port}")
        # server_sock.settimeout(1.0)
        while True:
            try:
                self.sock, addr = server_sock.accept()
                self.logger.info(f"Connection accepted from {addr}")
                self._configure_socket()
                break
            except socket.timeout:
                continue
        server_sock.close()

class RSPConnectorSerial(RSPConnector):
    def __init__(self, port, **kwargs):
        super().__init__(**kwargs)
        conn_timeout = kwargs.get('conn_timeout', 0.0001)
        baudrate = kwargs.get('baudrate', 115200)
        self.logger.info(f"Connecting to {port} as serial port @{baudrate} baud")
        self.com = serial.Serial(port, baudrate=baudrate, timeout=conn_timeout)
        self.com.reset_output_buffer()
        self.com.reset_input_buffer()

    def _read_impl(self, bufsize=4096):
        try:
            data = self.com.read(1)
            if not data:
                return b''
            data += self.com.read(min(self.com.in_waiting, bufsize - 1))
            return data
        except serial.SerialException as e:
            self.logger.error(f"Serial device disconnected or error: {e}")
            raise ConnectionResetError("Serial disconnected") from e
    
    def _write_impl(self, data):
        self.com.write(data)
        self.com.flush()

    def close(self):
        self.com.close()

class RSPProxy:
    class ProxyMiddleware(ABC):
        def __init__(self):
            self.logger = logging.getLogger(f'runners.{BlackMagicProbeRunner.name()}.{self.__class__.__name__}')

        def _request(self, *args): self.request(*args)
        def _reply(self, *args): self.reply(*args)
        
        def request(self, packet, ctx):
            ctx.request(packet)
        
        def reply(self, packet, ctx):
            ctx.reply(packet)

    class MiddlewareContext():
        def __init__(self, proxy):
            self.proxy = proxy
            self.chain = deque([proxy, *proxy.middlewares])
        
        def request(self, packet):
            self.chain.rotate(-1)
            self.chain[0]._request(packet, self)
        
        def reply(self, packet):
            self.chain.rotate(1)
            self.chain[0]._reply(packet, self)

    def __init__(self, server, client):
        self.logger = logging.getLogger(f'runners.{BlackMagicProbeRunner.name()}.{self.__class__.__name__}')
        self.server = server
        self.client = client
        self.middlewares = []
        self._running = False
    
    def _request(self, packet, _): self.server.txq.append(packet)
    def _forward_request(self, packet, ctx): ctx.request(packet)
    
    def _reply(self, packet, _): self.client.txq.append(packet)
    def _forward_reply(self, packet, ctx): ctx.reply(packet)

    def _spin_once(self, ctx = None):
        ctx = ctx or self.MiddlewareContext(self)
        self.server.spin()
        self.client.spin()
        # gather pending notifications/output from server side first
        while len(self.server.rxq):
            self._forward_reply(self.server.rxq.pop(), ctx)
        while len(self.client.rxq):
            self._forward_request(self.client.rxq.pop(), ctx)

    def stop(self): self._running = False

    def run(self):
        def signal_handler(_signum, _frame):
            self.logger.info("Termination signal received. Shutting down proxy.")
            self.stop()
            raise KeyboardInterrupt
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)

        ctx = self.MiddlewareContext(self)
        self._running = True
        try:
            while self._running: self._spin_once(ctx)
        except (ConnectionResetError, KeyboardInterrupt) as e:
            if isinstance(e, ConnectionResetError):
                self.logger.warning(f"Connection reset: {e}")
        finally:
            self.logger.debug("Stopping proxy server and client connections.")
            self.server.close()
            self.client.close()

class ProxyMiddlewareStartNoAckMode(RSPProxy.ProxyMiddleware):
    MAGIC_PACKET = RSPConnector.Packet('QStartNoAckMode')
    def __init__(self):
        super().__init__()
        self.awaiting_no_ack_reply = False

    def request(self, packet, ctx):
        if packet == self.MAGIC_PACKET:
            self.logger.info('NoAckMode requested; waiting for confirmation')
            self.awaiting_no_ack_reply = True
        ctx.request(packet)
    
    def reply(self, packet, ctx):
        if self.awaiting_no_ack_reply and type(packet) == RSPConnector.Packet and not packet.output:
            if packet == RSPConnector.Packet('OK'):
                self.logger.info('NoAckMode enabled')
                ctx.proxy.server.no_ack = True
                ctx.proxy.client.no_ack = True
            else:
                self.logger.info(f'NoAckMode not confirmed; got {packet.printable}')
            self.awaiting_no_ack_reply = False
        ctx.reply(packet)

class ProxyMiddlewareBMPTargetSoftReset(RSPProxy.ProxyMiddleware):
    MAGIC_PACKET = RSPConnector.Packet('monitor reset')

    def request(self, packet, ctx):
        if packet.payload.startswith(self.MAGIC_PACKET.payload):
            self.logger.info("Intercepted 'monitor reset'; requesting soft reset")
            ctx.request(RSPConnector.Packet('R'))
            ctx.reply(RSPConnector.Packet('OK'))
        else:
            ctx.request(packet)

# TODO: share prebuilt gdb commands between commands
def gdb_eval_prefix(commands, prefix='-ex'):
    return sum([[prefix, c] for c in commands], [])

class BlackMagicProbeRunner(ZephyrBinaryRunner):
    '''Runner front-end for Black Magic probe.'''

    def __init__(self, cfg,
                 gdb_serial=None,
                 rtt_serial=None,
                 connect_rst=False,
                 dev_id=None,
                 gdb_port=DEFAULT_BMDP_NET_GDB_PORT,
                 rtt=False,
                 quiet=False,
                 erase=True):
        super().__init__(cfg)
        self.gdb = [cfg.gdb] if cfg.gdb else None
        # as_posix() because gdb doesn't recognize backslashes as path
        # separators for the 'load' command we execute in bmp_flash().
        #
        # https://github.com/zephyrproject-rtos/zephyr/issues/50789
        self.elf_file = Path(cfg.elf_file).as_posix()
        if cfg.hex_file is not None:
            self.hex_file = Path(cfg.hex_file).as_posix()
        else:
            self.hex_file = None

        if sys.platform.startswith('darwin') and dev_id:
            self.logger.warning('dev_id paramter not implemented on darwin!')
        self.gdb_serial, self.rtt_serial = blackmagicprobe_ports(dev_id, gdb_serial, rtt_serial)
        self.gdb_port = gdb_port
        if connect_rst:
            self.connect_rst_enable_arg = [
                    "monitor connect_rst enable",
                    "monitor connect_srst enable",
                    ]
            self.connect_rst_disable_arg = [
                    "monitor connect_rst disable",
                    "monitor connect_srst disable",
                    ]
        else:
            self.connect_rst_enable_arg = []
            self.connect_rst_disable_arg = []
        if erase:
            self.logger.info(f'erasing full flash')
            self.erase_arg = [
                    "monitor erase_mass",
                    ]
        else:
            self.erase_arg = []
        self.quiet = quiet
        if rtt:
            self.rtt_arg = [
                "monitor rtt enable",
                "monitor rtt ram",
            ]
        else:
            self.rtt_arg = [
                "monitor rtt disable"
            ]

    @classmethod
    def name(cls):
        return 'blackmagicprobe'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash', 'debug', 'attach', 'rtt', 'debugserver'},
                          dev_id=True, erase=True)

    @classmethod
    def do_create(cls, cfg, args):
        return BlackMagicProbeRunner(cfg,
                                     gdb_serial=args.gdb_serial,
                                     rtt_serial=args.rtt_serial,
                                     connect_rst=args.connect_rst,
                                     dev_id=args.dev_id,
                                     gdb_port=args.gdb_port,
                                     rtt=args.rtt,
                                     quiet=args.quiet,
                                     erase=args.erase)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--gdb-serial', help='GDB serial port')
        parser.add_argument('--rtt-serial', help='RTT/UART serial port')
        parser.add_argument('--connect-rst', '--connect-srst', action='store_true',
                            help='Assert SRST during connect? (default: no)')
        parser.add_argument('--id', required=False, dest='dev_id',
                            help='obsolete synonym for -i/--dev-id')
        parser.add_argument('--gdb-port', default=DEFAULT_BMDP_NET_GDB_PORT, type=int,
                            help=f'port to expose gdb server, defaults to {DEFAULT_BMDP_NET_GDB_PORT}')
        parser.add_argument('--rtt', action='store_true',
                            help='Enable RTT (default: no)')
        parser.add_argument('--quiet', action='store_true',
                            help='Silence logging for RTT (default: no)')


    def bmp_debugserver(self, command="", **kwargs):
        probe_conn = RSPConnectorSerial(self.gdb_serial, baudrate=115200)
        gdb = GDBRSPClient(probe_conn)

        debugserver_gdb_init_comands = ([
            # "!",
            "monitor"] +
            self.connect_rst_enable_arg +
            ["monitor swdp_scan",
            "vAttach;1"] +
            self.rtt_arg
        )

        for gdb_command in debugserver_gdb_init_comands:
            res, out = gdb.execute(gdb_command)
            self.logger.debug(f"<- {gdb_command}")
            if out is not None and len(out):
                self.logger.info(out)
            if res[0] == 'E':
                raise RuntimeError(f'{gdb_command}: {res}')

        del gdb

        proxy = RSPProxy(probe_conn, RSPConnectorTCPServer('', self.gdb_port))
        proxy.middlewares = [ProxyMiddlewareStartNoAckMode(), ProxyMiddlewareBMPTargetSoftReset()]
        proxy.run()

    def bmp_rtt(self, command, **kwargs):
        rtt_port = serial.serial_for_url(self.rtt_serial, 115200, do_not_open=True)
        if not hasattr(rtt_port, 'cancel_read'):
            rtt_port.timeout = 1
        rtt_port.open()

        miniterm = Miniterm(rtt_port)
        miniterm.set_rx_encoding('UTF-8')
        miniterm.set_tx_encoding('UTF-8')
        miniterm.exit_character = chr(0x03)

        if not self.quiet:
            self.logger.info(f'RTT console on {self.rtt_serial} {miniterm.serial.baudrate},{miniterm.serial.bytesize},{miniterm.serial.parity},{miniterm.serial.stopbits} UTF-8')

        miniterm.start()
        try:
            miniterm.join(True)
        except KeyboardInterrupt:
            pass
        if not self.quiet:
            print()
            self.logger.info('RTT console exit')
        miniterm.join()
        miniterm.close()

    def bmp_flash(self, command, **kwargs):
        # if hex file is present and signed, use it else use elf file
        if self.hex_file:
            split = self.hex_file.split('.')
            # eg zephyr.signed.hex
            if len(split) >= 3 and split[-2] == 'signed':
                flash_file = self.hex_file
            else:
                flash_file = self.elf_file
        else:
            flash_file = self.elf_file

        if flash_file is None:
            raise ValueError('Cannot flash; elf file is missing')

        gdb_commands = (
                    ["set confirm off",
                     "set pagination off",
                     f"target extended-remote {self.gdb_serial}"] +
                     self.connect_rst_enable_arg +
                    ["monitor swdp_scan",
                     "attach 1"] +
                     self.erase_arg +
                    [f"load {flash_file}",
                     "kill",
                     "quit"])

        command = (self.gdb + gdb_eval_prefix(gdb_commands) + ['-silent'])
        self.check_call(command)

    def check_call_ignore_sigint(self, command):
        previous = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            self.check_call(command)
        finally:
            signal.signal(signal.SIGINT, previous)

    def bmp_attach(self, command, **kwargs):
        if self.elf_file is None:
            gdb_commands = (
                       ["set confirm off",
                        f"target extended-remote {self.gdb_serial}"] +
                        self.connect_rst_disable_arg +
                       ["monitor swdp_scan",
                        "attach 1"] +
                        self.rtt_arg)
        else:
            gdb_commands = (
                        ["set confirm off",
                         f"target extended-remote {self.gdb_serial}"] +
                         self.connect_rst_disable_arg +
                        ["monitor swdp_scan",
                         "attach 1"] +
                         self.rtt_arg +
                         [f"file {self.elf_file}"])

        command = (self.gdb + gdb_eval_prefix(gdb_commands))
        self.check_call_ignore_sigint(command)

    def bmp_debug(self, command, **kwargs):
        if self.elf_file is None:
            raise ValueError('Cannot debug; elf file is missing')
        gdb_commands = (
                   ["set confirm off",
                    f"target extended-remote {self.gdb_serial}"] +
                    self.connect_rst_enable_arg +
                   ["monitor swdp_scan",
                    "attach 1"] +
                    self.rtt_arg +
                    [f"file {self.elf_file}",
                    f"load {self.elf_file}"])

        command = (self.gdb + gdb_eval_prefix(gdb_commands)) # + ['-silent'])
        self.check_call_ignore_sigint(command)

    def do_run(self, command, **kwargs):
        if MISSING_REQUIREMENTS:
            raise RuntimeError('one or more Python dependencies are missing; '
                               'please check requirements depending on your platform')

        if command in ['flash', 'debug', 'attach']:
            if self.gdb is None:
                raise ValueError('Cannot execute; gdb not specified')
            self.require(self.gdb[0])
            
        if command in ['flash', 'debug', 'attach', 'debugserver']:
            if self.gdb_serial is None:
                raise ValueError('Cannot execute; BMDP gdb serial port not found')
            self.logger.info(f'using GDB serial: {self.gdb_serial}')

        if command in ['rtt']:
            if self.rtt_serial is None:
                raise ValueError('Cannot execute; BMDP RTT serial port not found')

        if command == 'flash':
            self.bmp_flash(command, **kwargs)
        elif command == 'debug':
            self.bmp_debug(command, **kwargs)
        elif command == 'attach':
            self.bmp_attach(command, **kwargs)
        elif command == 'debugserver':
            self.bmp_debugserver(command, **kwargs)
        elif command == 'rtt':
            self.bmp_rtt(command, **kwargs)
        else:
            self.bmp_flash(command, **kwargs)
