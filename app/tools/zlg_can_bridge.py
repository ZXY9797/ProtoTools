#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import os
import queue
import sys
import threading
import time
from pathlib import Path

OK = 1
TYPE_CAN = 0
TYPE_CANFD = 1
MAX_CAN = 8
MAX_CANFD = 64


class CanInit(ctypes.Structure):
    _fields_ = [
        ("acc_code", ctypes.c_uint),
        ("acc_mask", ctypes.c_uint),
        ("reserved", ctypes.c_uint),
        ("filter", ctypes.c_ubyte),
        ("timing0", ctypes.c_ubyte),
        ("timing1", ctypes.c_ubyte),
        ("mode", ctypes.c_ubyte),
    ]


class CanFdInit(ctypes.Structure):
    _fields_ = [
        ("acc_code", ctypes.c_uint),
        ("acc_mask", ctypes.c_uint),
        ("abit_timing", ctypes.c_uint),
        ("dbit_timing", ctypes.c_uint),
        ("brp", ctypes.c_uint),
        ("filter", ctypes.c_ubyte),
        ("mode", ctypes.c_ubyte),
        ("pad", ctypes.c_ushort),
        ("reserved", ctypes.c_uint),
    ]


class InitUnion(ctypes.Union):
    _fields_ = [("can", CanInit), ("canfd", CanFdInit)]


class ChannelInit(ctypes.Structure):
    _fields_ = [("can_type", ctypes.c_uint), ("config", InitUnion)]


class CanFrame(ctypes.Structure):
    _fields_ = [
        ("can_id", ctypes.c_uint, 29),
        ("err", ctypes.c_uint, 1),
        ("rtr", ctypes.c_uint, 1),
        ("eff", ctypes.c_uint, 1),
        ("can_dlc", ctypes.c_ubyte),
        ("pad", ctypes.c_ubyte),
        ("reserved0", ctypes.c_ubyte),
        ("reserved1", ctypes.c_ubyte),
        ("data", ctypes.c_ubyte * MAX_CAN),
    ]


class TxCan(ctypes.Structure):
    _fields_ = [("frame", CanFrame), ("transmit_type", ctypes.c_uint)]


class RxCan(ctypes.Structure):
    _fields_ = [("frame", CanFrame), ("timestamp", ctypes.c_ulonglong)]


class CanFdFrame(ctypes.Structure):
    _fields_ = [
        ("can_id", ctypes.c_uint, 29),
        ("err", ctypes.c_uint, 1),
        ("rtr", ctypes.c_uint, 1),
        ("eff", ctypes.c_uint, 1),
        ("length", ctypes.c_ubyte),
        ("brs", ctypes.c_ubyte, 1),
        ("esi", ctypes.c_ubyte, 1),
        ("reserved", ctypes.c_ubyte, 6),
        ("reserved0", ctypes.c_ubyte),
        ("reserved1", ctypes.c_ubyte),
        ("data", ctypes.c_ubyte * MAX_CANFD),
    ]


class TxFd(ctypes.Structure):
    _fields_ = [("frame", CanFdFrame), ("transmit_type", ctypes.c_uint)]


class RxFd(ctypes.Structure):
    _fields_ = [("frame", CanFdFrame), ("timestamp", ctypes.c_ulonglong)]


CAN_TIMING = {
    1000000: (0x00, 0x14),
    800000: (0x00, 0x16),
    500000: (0x00, 0x1C),
    250000: (0x01, 0x1C),
    125000: (0x03, 0x1C),
    100000: (0x04, 0x1C),
    50000: (0x09, 0x1C),
}


def load_lib(path: str):
    dll_path = Path(path)
    if dll_path.exists() and os.name == "nt" and hasattr(os, "add_dll_directory"):
        os.add_dll_directory(str(dll_path.parent))
        kernel_dir = dll_path.parent / "kerneldlls"
        if kernel_dir.exists():
            os.add_dll_directory(str(kernel_dir))
    return ctypes.WinDLL(str(dll_path)) if os.name == "nt" else ctypes.CDLL(str(dll_path))


def bind(lib) -> None:
    lib.ZCAN_OpenDevice.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
    lib.ZCAN_OpenDevice.restype = ctypes.c_void_p
    lib.ZCAN_CloseDevice.argtypes = [ctypes.c_void_p]
    lib.ZCAN_CloseDevice.restype = ctypes.c_uint
    lib.ZCAN_InitCAN.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ChannelInit)]
    lib.ZCAN_InitCAN.restype = ctypes.c_void_p
    lib.ZCAN_StartCAN.argtypes = [ctypes.c_void_p]
    lib.ZCAN_StartCAN.restype = ctypes.c_uint
    lib.ZCAN_ResetCAN.argtypes = [ctypes.c_void_p]
    lib.ZCAN_ResetCAN.restype = ctypes.c_uint
    lib.ZCAN_Transmit.argtypes = [ctypes.c_void_p, ctypes.POINTER(TxCan), ctypes.c_uint]
    lib.ZCAN_Transmit.restype = ctypes.c_uint
    lib.ZCAN_Receive.argtypes = [ctypes.c_void_p, ctypes.POINTER(RxCan), ctypes.c_uint, ctypes.c_int]
    lib.ZCAN_Receive.restype = ctypes.c_uint
    lib.ZCAN_TransmitFD.argtypes = [ctypes.c_void_p, ctypes.POINTER(TxFd), ctypes.c_uint]
    lib.ZCAN_TransmitFD.restype = ctypes.c_uint
    lib.ZCAN_ReceiveFD.argtypes = [ctypes.c_void_p, ctypes.POINTER(RxFd), ctypes.c_uint, ctypes.c_int]
    lib.ZCAN_ReceiveFD.restype = ctypes.c_uint
    if hasattr(lib, "ZCAN_SetValue"):
        lib.ZCAN_SetValue.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        lib.ZCAN_SetValue.restype = ctypes.c_uint


def set_value(lib, device, channel: int, key: str, value: int) -> None:
    if not hasattr(lib, "ZCAN_SetValue"):
        return
    path = f"{channel}/{key}".encode("ascii")
    encoded = str(value).encode("ascii")
    if lib.ZCAN_SetValue(device, path, encoded) != OK:
        raise RuntimeError(f"ZCAN_SetValue failed: {path.decode()}={value}")


def make_config(args) -> ChannelInit:
    cfg = ChannelInit()
    if args.format == "can":
        t0, t1 = CAN_TIMING.get(args.arb_baud, CAN_TIMING[500000])
        cfg.can_type = TYPE_CAN
        cfg.config.can.acc_code = 0
        cfg.config.can.acc_mask = 0xFFFFFFFF
        cfg.config.can.reserved = 0
        cfg.config.can.filter = 0
        cfg.config.can.timing0 = t0
        cfg.config.can.timing1 = t1
        cfg.config.can.mode = 0
    else:
        cfg.can_type = TYPE_CANFD
        cfg.config.canfd.acc_code = 0
        cfg.config.canfd.acc_mask = 0xFFFFFFFF
        cfg.config.canfd.abit_timing = args.abit_timing
        cfg.config.canfd.dbit_timing = args.dbit_timing
        cfg.config.canfd.brp = 0
        cfg.config.canfd.filter = 0
        cfg.config.canfd.mode = 0
        cfg.config.canfd.pad = 0
        cfg.config.canfd.reserved = 0
    return cfg


def open_channel(args):
    lib = load_lib(args.zlg_lib)
    bind(lib)
    device = lib.ZCAN_OpenDevice(args.device_type, args.device_index, 0)
    if not device:
        raise RuntimeError("ZCAN_OpenDevice failed")
    channel = None
    try:
        if args.format == "can":
            set_value(lib, device, args.channel, "protocol", 0)
            set_value(lib, device, args.channel, "can_baud_rate", args.arb_baud)
        else:
            set_value(lib, device, args.channel, "protocol", 1)
            set_value(lib, device, args.channel, "canfd_standard", 0)
            set_value(lib, device, args.channel, "clock", 60000000)
            set_value(lib, device, args.channel, "canfd_abit_baud_rate", args.arb_baud)
            set_value(lib, device, args.channel, "canfd_dbit_baud_rate", args.data_baud)
        cfg = make_config(args)
        channel = lib.ZCAN_InitCAN(device, args.channel, ctypes.byref(cfg))
        if not channel:
            raise RuntimeError("ZCAN_InitCAN failed")
        if lib.ZCAN_StartCAN(channel) != OK:
            raise RuntimeError("ZCAN_StartCAN failed")
        return lib, device, channel
    except Exception:
        if channel:
            lib.ZCAN_ResetCAN(channel)
        lib.ZCAN_CloseDevice(device)
        raise


def send_can(lib, channel, args, payload: bytes) -> None:
    limit = min(args.payload_size, MAX_CAN if args.format == "can" else MAX_CANFD)
    for offset in range(0, len(payload), limit):
        chunk = payload[offset:offset + limit]
        if args.format == "can":
            msg = TxCan()
            msg.transmit_type = args.transmit_type
            msg.frame.can_id = args.tx_id
            msg.frame.eff = 1 if args.extended else 0
            msg.frame.can_dlc = len(chunk)
            for i, value in enumerate(chunk):
                msg.frame.data[i] = value
            sent = lib.ZCAN_Transmit(channel, ctypes.byref(msg), 1)
        else:
            msg = TxFd()
            msg.transmit_type = args.transmit_type
            msg.frame.can_id = args.tx_id
            msg.frame.eff = 1 if args.extended else 0
            msg.frame.brs = 1 if args.brs else 0
            msg.frame.length = len(chunk)
            for i, value in enumerate(chunk):
                msg.frame.data[i] = value
            sent = lib.ZCAN_TransmitFD(channel, ctypes.byref(msg), 1)
        if sent != 1:
            raise RuntimeError(f"transmit failed: sent={sent}")
        if args.inter_frame_delay_ms > 0:
            time.sleep(args.inter_frame_delay_ms / 1000.0)


def receiver(lib, channel, args, stop: threading.Event, errors: queue.Queue) -> None:
    try:
        while not stop.is_set():
            if args.format == "can":
                frames = (RxCan * 16)()
                count = lib.ZCAN_Receive(channel, frames, len(frames), args.poll_ms)
                for i in range(min(count, len(frames))):
                    frame = frames[i].frame
                    if frame.err or frame.rtr or bool(frame.eff) != args.extended or frame.can_id != args.rx_id:
                        continue
                    data = bytes(frame.data[:min(frame.can_dlc, MAX_CAN)])
                    print("RX " + data.hex(" ").upper(), flush=True)
            else:
                frames = (RxFd * 16)()
                count = lib.ZCAN_ReceiveFD(channel, frames, len(frames), args.poll_ms)
                for i in range(min(count, len(frames))):
                    frame = frames[i].frame
                    if frame.err or frame.rtr or bool(frame.eff) != args.extended or frame.can_id != args.rx_id:
                        continue
                    data = bytes(frame.data[:min(frame.length, MAX_CANFD)])
                    print("RX " + data.hex(" ").upper(), flush=True)
    except Exception as exc:
        errors.put(exc)
        print(f"ERR {exc}", flush=True)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zlg-lib", required=True)
    parser.add_argument("--device-type", type=int, default=41)
    parser.add_argument("--device-index", type=int, default=0)
    parser.add_argument("--channel", type=int, default=0)
    parser.add_argument("--tx-id", type=lambda s: int(s, 0), default=0x100)
    parser.add_argument("--rx-id", type=lambda s: int(s, 0), default=0x200)
    parser.add_argument("--format", choices=("can", "canfd"), default="canfd")
    parser.add_argument("--extended", action="store_true")
    parser.add_argument("--brs", action="store_true")
    parser.add_argument("--transmit-type", type=int, default=0)
    parser.add_argument("--arb-baud", type=int, default=1000000)
    parser.add_argument("--data-baud", type=int, default=1000000)
    parser.add_argument("--abit-timing", type=int, default=101166)
    parser.add_argument("--dbit-timing", type=int, default=101166)
    parser.add_argument("--payload-size", type=int, default=64)
    parser.add_argument("--poll-ms", type=int, default=5)
    parser.add_argument("--inter-frame-delay-ms", type=int, default=0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    lib, device, channel = open_channel(args)
    stop = threading.Event()
    errors: queue.Queue = queue.Queue()
    thread = threading.Thread(target=receiver, args=(lib, channel, args, stop, errors), daemon=True)
    thread.start()
    print("OPENED", flush=True)
    try:
        for raw in sys.stdin:
            line = raw.strip()
            if not line:
                continue
            if line == "CLOSE":
                break
            if not line.startswith("TX "):
                print(f"ERR unknown command: {line}", flush=True)
                continue
            payload = bytes.fromhex(line[3:])
            send_can(lib, channel, args, payload)
            print(f"TXOK {len(payload)}", flush=True)
            if not errors.empty():
                raise errors.get()
    finally:
        stop.set()
        thread.join(timeout=0.5)
        lib.ZCAN_ResetCAN(channel)
        lib.ZCAN_CloseDevice(device)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERR {exc}", flush=True)
        raise SystemExit(2)
