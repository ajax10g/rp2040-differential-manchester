from __future__ import print_function

import hid
from time import sleep
import random
import threading
import typing
from typing import Callable, Any
from dataclasses import dataclass
import os


@dataclass
class HidIntf:
    h: hid.device
    kill_thread = False
    read_bytecount = 0
    write_bytecount = 0

    @classmethod
    def print_hexified_buffer(cls, buffer: list[int]):
        bufsize = len(buffer)
        hexified = ' '.join([f"{x:02x}" for x in buffer])
        print(f"len={bufsize: 3d}:\n{hexified}")

    def clear_bytecount(self):
        self.read_bytecount = 0
        self.write_bytecount = 0

    def print_bytecount(self):
        print(f"Total Write Byte Count = {self.write_bytecount}")
        print(f"Total Read Byte Count  = {self.read_bytecount}")

    def read_and_print(self) -> None:
        buf = self.h.read(64)
        self.print_hexified_buffer(buf)

    def loop_write(
        self,
        count: int = 100,
        byte_count: int = 64,
        write_interval: float = 0.00,
        generator: Any = None,
        write_padding: bool = True,
        verbose: bool = True,
    ) -> None:
        for _ in range(count):
            match generator:
                case bytes() | bytearray():
                    words = list(generator)
                case list():
                    words = generator
                case _:
                    if isinstance(generator, Callable):
                        words = generator(byte_count)
                    else:
                        words = [random.randint(0, 255) for _ in range(byte_count)]

            # Pad with zeros if words are not multiple of 64 bytes
            if write_padding:
                words_len = len(words)
                remainder = words_len % 64
                padding_count = 64 - remainder if remainder else 0
                words += [0] * padding_count

            # FIRST BYTE IS NOT ZERO
            # __import__('pdb').set_trace()
            if words[0] == 0:
                words[0] = 0xFF

            if verbose:
                print(f"WR:", end=" ")
                self.print_hexified_buffer(words)
            self.h.write(words)
            self.write_bytecount += len(words)

            if write_interval > 0.0:
                sleep(write_interval)

    def loop_read(self, timeout_ms: int = 0, verbose: bool = True) -> None:
        while not self.kill_thread:
            buffer = self.h.read(64, timeout_ms=timeout_ms)
            running_count = len(buffer)
            if running_count > 1:
                if verbose:
                    print(f"RD:", end=" ")
                    self.print_hexified_buffer(buffer)
                self.read_bytecount += running_count

    def loop_write_read(
        self,
        count: int = 1,
        byte_count: int = 64,
        write_interval: float = 0.100,
        read_timeout_ms: int = 0,
        generator: Any | None = None,
        write_padding: bool = True,
        verbose: bool = True,
    ) -> None:
        self.kill_thread = False
        thread_read = threading.Thread(
            target=self.loop_read,
            args=(
                read_timeout_ms,
                verbose,
            ),
        )
        thread_write = threading.Thread(
            target=self.loop_write,
            args=(
                count,
                byte_count,
                write_interval,
                generator,
                write_padding,
                verbose,
            ),
        )

        thread_read.start()
        sleep(0.1)
        thread_write.start()

        # Wait until thread is completly executed
        thread_write.join()

        sleep(0.1)
        self.kill_thread = True

        self.print_bytecount()


# enumerate USB devices

# for d in hid.enumerate():
#     keys = list(d.keys())
#     keys.sort()
#     for key in keys:
#         print("%s : %s" % (key, d[key]))
#     print()

# try opening a device, then perform write and read

try:
    print("Opening the device")

    h = hid.device()
    h.open(
        vendor_id=0xCAFE,
        product_id=0x4004,
        serial_number=os.getenv('SERIAL') or "DE623134CF856234",
    )
    # h.open(0xCAFE, 0x4004)  # VendorID/ProductID

    print("Manufacturer: %s" % h.get_manufacturer_string())
    print("Product: %s" % h.get_product_string())
    print("Serial No: %s" % h.get_serial_number_string())

    # enable non-blocking mode
    h.set_nonblocking(1)

    # # write some data to the device
    # print("Write the data")
    # h.write([0, 63, 35, 35] + [0] * 61)

    # wait
    sleep(0.05)

    # Interface to HID
    hif = HidIntf(h)

    # # read back the answer
    # print("Read the data")
    # while True:
    #     d = h.read(64)
    #     if d:
    #         print(d)
    #     else:
    #         break

    # __import__('pdb').set_trace()
    # print("Closing the device")
    # h.close()

except IOError as ex:
    print(ex)
    print("You probably don't have the hard-coded device.")
    print("Update the h.open() line in this script with the one")
    print("from the enumeration list output above and try again.")

print("Done")
