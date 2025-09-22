#!/usr/bin/env python3
"""
flash_tool.py – Utility for reading and writing flash memory via Arduino UART commands.

Usage:
    python flash_tool.py <command> [options] <offset> <file>

Commands:
    read   Read flash contents starting at <offset> and write to <file>.
    write  Write flash contents from <file> starting at <offset>.

Options:
    --port PORT   Serial device path (default: /dev/ttyUSB0).
"""

import argparse
import os
import sys
import time

import logging
from tqdm import tqdm

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

try:
    import serial
except ImportError:
    logger.error("pyserial is required. Install with 'pip install pyserial'.")
    sys.exit(1)

PAGE_SIZE = 256
FLASH_SIZE = 2 * 1024 * 1024  # 2 MiB

def open_serial(port: str, baudrate: int = 115200):
    """Open the serial port with the required parameters."""
    try:
        ser = serial.Serial(port, baudrate=baudrate, timeout=3)
        # Give the board time to reset after opening the port
        time.sleep(5)
        # Flush any leftover data from the serial buffers
        clean_state(ser)
        return ser
    
    except serial.SerialException as e:
        logger.error(f"Failed to open serial port {port}: {e}")
        sys.exit(1)

def clean_state(conn: serial.Serial):
    conn.reset_input_buffer()
    conn.reset_output_buffer()
    
    conn.write("\x03".encode("utf-8"))
    conn.read(4)
    conn.flush()
    

def read_page(conn: serial.Serial, address: int) -> bytes | None:
    """
    Read a single flash page (256 bytes) from the given address using a non‑blocking approach.

    Sends the read command immediately, then repeatedly reads any available bytes from the serial connection using ``conn.in_waiting`` until the full page is collected or a total timeout of 10 seconds expires. A tiny ``time.sleep(0.001)`` pause is used only when no data is available to avoid a busy‑loop.

    Returns the page data as ``bytes`` or ``None`` on timeout.
    The operation aborts after a 10‑second timeout, logging a warning and returning None.
    """
    
    clean_state(conn)
    
    cmd = f"read-flash-page-raw 0x{address:06X}\n".encode()
    logger.debug(f"Sending command: {cmd}")
    conn.write(cmd)
    conn.flush()
    echo = conn.read(len(cmd)+1) # Command echo
    
    if chr(echo[-1]) != '\n':
        print("Bad echo!", echo)
        return None
    
    #print("DBG:", len(cmd), cmd, echo)

    # Use a timeout that resets after each successful read chunk
    timeout = 3.0               # seconds – generous for slower responses
    data = bytearray()
    last_progress = time.time()   # timestamp of the last successful read
    
    data = conn.read(PAGE_SIZE)

    if len(data) != PAGE_SIZE:
        logger.warning(f"Incomplete page read at 0x{address:08X}")
        print(data)
        return None

    logger.debug(f"Successfully read full page at 0x{address:08X}")
        
    return bytes(data)

def write_page(conn: serial.Serial, address: int, data: bytes):
    """Write a single flash page (256 bytes) to the given address."""
    
    if len(data) != PAGE_SIZE:
        logger.error("write_page called with incorrect data length.")
        sys.exit(1)
        
        clean_state(conn)
    
    cmd = f"write-flash-page-raw 0x{address:06X}\n".encode()
    logger.debug(f"Sending command: {cmd}")
    conn.write(cmd)
    conn.flush()
    echo = conn.read(len(cmd)+1) # Command echo
    
    if chr(echo[-1]) != '\n':
        logger.debug(f"Bad echo! {echo}")
        return False
    
    ready_ok = conn.read(7)
    
    if ready_ok != b"READY\r\n":
        logger.debug(f"Bad echo! {ready_ok}")
        return False
        
        
    conn.write(data)
    
    #for i, byte in enumerate(data):
    #    #print(i, byte)
    #    conn.write([byte])
        
    write_ok = conn.read(4)
    
    if write_ok != b"OK\r\n":
        logger.debug(f"Bad echo! {write_ok}")
        return False
    
    #time.sleep(5)
                
    new_data = read_page(conn, address)
    
    print(data)
    print(new_data)
        
    if data == new_data:
        logger.debug("Write verification correct")
        return True

    return False
    
def read_flash(conn: serial.Serial, offset: int, out_path: str, size: int = None):
    """Read flash from offset to the end of memory and save to a file."""
    if size is not None:
        total_pages = size // PAGE_SIZE
    else:
        total_pages = (FLASH_SIZE - offset) // PAGE_SIZE
        
    with open(out_path, "wb") as out_file:
        for i in tqdm(range(total_pages), desc="Reading", unit="page"):
            addr = offset + i * PAGE_SIZE
            logger.debug(f"Reading page {i+1}/{total_pages} at 0x{addr:08X}")
            data = read_page(conn, addr)
            if data is None:
                logger.error(f"Terminating read due to incomplete page at 0x{addr:08X}")
                break
            out_file.write(data)
            
def write_flash(conn: serial.Serial, offset: int, in_path: str):
    """Write the contents of a file to flash starting at the given offset."""
    size = os.path.getsize(in_path)
    total_pages = size // PAGE_SIZE
    with open(in_path, "rb") as in_file:
        for i in tqdm(range(total_pages), desc="Writing", unit="page"):
            addr = offset + i * PAGE_SIZE
            data = in_file.read(PAGE_SIZE)
            if len(data) != PAGE_SIZE:
                logger.error(f"Unexpected end of file at page {i+1}.")
                sys.exit(1)
            logger.debug(f"Writing page {i+1}/{total_pages} at 0x{addr:08X}")
            
            if(not write_page(conn, addr, data)):
                logger.error(f"Terminating write due to verify error page at 0x{addr:08X}")
                break

def validate_offset(offset: int):
    """Common offset validation."""
    if offset < 0:
        logger.error("Offset must be non‑negative.")
        sys.exit(1)
    if offset % PAGE_SIZE != 0:
        logger.error(f"Offset must be page‑aligned (multiple of {PAGE_SIZE}).")
        sys.exit(1)
    if offset >= FLASH_SIZE:
        logger.error("Offset exceeds flash size.")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Flash memory utility via UART.")
    parser.add_argument("--port", default="/dev/ttyUSB0",
                        help="Serial device (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Enable verbose output")
    
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    # Read command
    read_parser = subparsers.add_parser("read", help="Read flash to a file")
    read_parser.add_argument("offset", type=int, help="Flash offset (page‑aligned)")
    read_parser.add_argument("file", help="Output file path")
    read_parser.add_argument("--size", type=str, help="Size to read (hex: 0x100, decimal: 256)")
    
    # Write command
    write_parser = subparsers.add_parser("write", help="Write flash from a file")
    write_parser.add_argument("offset", type=int, help="Flash offset (page‑aligned)")
    write_parser.add_argument("file", help="Input file path")
    
    args = parser.parse_args()
    
    # Configure logging level based on verbose flag
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logger.setLevel(log_level)
    
    validate_offset(args.offset)
    
    conn = open_serial(args.port, args.baud)

    
    
    if args.command == "read":
        if args.size:
            if args.size.startswith('0x'):
                size_bytes = int(args.size, 16)
            else:
                size_bytes = int(args.size)
            read_flash(conn, args.offset, args.file, size=size_bytes)
        else:
            read_flash(conn, args.offset, args.file)
    else:  # write
        if not os.path.isfile(args.file):
            logger.error(f"Input file '{args.file}' does not exist.")
            sys.exit(1)
        file_size = os.path.getsize(args.file)
        if file_size % PAGE_SIZE != 0:
            logger.error("Input file size must be a multiple of 256 bytes.")
            sys.exit(1)
        if args.offset + file_size > FLASH_SIZE:
            logger.error("Write operation would exceed flash size.")
            sys.exit(1)
        write_flash(conn, args.offset, args.file)
    
    conn.close()
    sys.exit(0)
    
if __name__ == "__main__":
    main()