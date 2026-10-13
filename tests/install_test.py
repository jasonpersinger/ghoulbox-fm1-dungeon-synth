#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device is the one in web/test_web.mjs: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node and
build/felucca.fwsc, skipped otherwise), and (BOARD_SMK37) the SMK-37 Pro identities and port names and the
20 / 36 block deinterleave (the stock SMK-37 Pro V016 from a directory named by SMK_STOCK_DIR, skipped when unset)."""
import io
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, loader=("ota-FM-1_900", "Felucca Update")):
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.loader = loader
        self.stall_after, self.bad_addr = stall_after, bad_addr
        self.served = self.bad = self.upgrades = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, self.loader).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_900", marker=True, size=0x2000):
    raw = bytearray((i * 7) & 0xFF for i in range(size + I.BLOCKS))
    for i in range(I.BLOCKS):
        raw[i * I.BLK + I.KEEP] = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
    raw[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    return bytes(raw)


def package_crc(product="FM-1_900", size=0x3000, blocks=20, seed=7):
    """a package with a valid header and file list (one file, flash.bin: 0x400 to the end), as fm1pkg_make writes,
    in `blocks` interleave blocks (the stock SMK-37 Pro V016: 36)"""
    img = bytearray((i * seed) & 0xFF for i in range(size))
    img[0x1800:0x1800 + 16] = I.LOADER_MARK
    data = bytes(img[0x400:])
    e = bytearray(0x50)
    e[0:16] = bytes(4) + I._crc16(data).to_bytes(2, "little") + bytes(2) + \
        (0x400).to_bytes(4, "little") + len(data).to_bytes(4, "little")
    e[0x40:0x49] = b"flash.bin"
    lst = I._dec(bytes(e))
    hdr = bytearray(0x40)
    hdr[4:8] = size.to_bytes(4, "little")
    hdr[8:10] = (1).to_bytes(2, "little")
    hdr[2:4] = I._crc16(lst).to_bytes(2, "little")
    hdr[0:2] = I._crc16(bytes(hdr[2:0x40])).to_bytes(2, "little")
    img[0:0x40] = I._dec(bytes(hdr))
    img[0x40:0x90] = lst
    raw = bytearray()
    for i in range(blocks):
        m = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
        raw += img[i * I.KEEP:(i + 1) * I.KEEP] + bytes([m])
    raw += img[blocks * I.KEEP:]
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))


def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")

    dev = FakeFM1(image, identity="\x1b[2J\x1b]0;x\x07FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and not any(c in out for c in "\x1b\x07"),
       "--info: control bytes in the device's identity are not printed")
    import fm1_sample_upload as U
    lines = U.slot_info([3, 80, 2, 0x41, 0x1B, 0x5B, 0x32, 0x4A, 0, 40, 0, 0, 0, 0, 0, 0])
    ok(lines[0] == "USR1: 2 zones, A?[2J, 40 KiB" and lines[1] == "USR2: empty", "sample upload: slot names printable only")
    try:
        U.slot_info([3, 80, 2, 0x41])
        ok(False, "sample upload: a malformed SMP_INFO reply is an error")
    except ValueError:
        ok(True, "sample upload: a malformed SMP_INFO reply is an error")


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", unplug_after=3))
    ok(rc == 4 and "run the install again" in err and time.monotonic() - t0 < 3, "unplugged during the write -> exit 4 at once")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", stall_after=3))
    ok(rc == 4 and "stopped answering" in err, "loader stops answering -> exit 4 after the idle time")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 0 and dev.bad == 0, "... installs with --force")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def against_js():
    clean = ROOT / "build/felucca.fwsc"
    if not shutil.which("node") or not clean.exists():
        print("logical image vs fm1pkg.js: skipped (needs node and build/felucca.fwsc)")
        return
    js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
          "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
    r = subprocess.run(["node", "--input-type=module", "-e", js, str(clean)], capture_output=True, check=True)
    raw = clean.read_bytes()
    ok(I.logical_image(raw) == r.stdout and I.product_of(raw) == r.stderr.decode(),
       f"logical_image / product_of == fm1pkg.js ({clean.name}, {I.product_of(raw)})")
    ok(I.LOADER_MARK in raw, f"{clean.name} carries the Felucca loader marker")


def official():
    """#32: back to the official V15 from Felucca, without --force (only the unmodified file)"""
    v15 = ROOT / "FM-1_v15.fwsc"
    if not v15.exists():
        print("official V15 restore: skipped (needs FM-1_v15.fwsc in the repo root)")
        return
    raw = v15.read_bytes()
    dev = FakeFM1(I.logical_image(raw), identity="FM-1_900", name="Felucca", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "FM-1_015" in out and dev.identity == "FM-1_015",
       "official V15 (FM-1.fwsc) from Felucca: installed without --force, back as FM-1_015")
    bad = bytearray(raw)
    bad[-1] ^= 1
    dev = FakeFM1(I.logical_image(bytes(bad)), identity="FM-1_900", name="Felucca")
    rc, out, err = cli([pkgfile("v15mod.fwsc", bytes(bad)), "--yes"], dev)
    ok(rc == 2 and "official V15" in err and dev.sent == [], "a modified V15 is still refused (no MIDI)")


def smk():
    """SMK-37 Pro: identities, port names, the 36-block stock package, the SMK install path"""
    ok(I.PORT_RE.search("SMK-37 Pro Midi") and I.PORT_RE.search("FM-1") and not I.PORT_RE.search("IAC Driver Bus 1"),
       "port names: SMK-37 Pro ports probed, FM-1 too, other gear not")
    ident = I.parse_identity(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + b"SMK-37 Pro_016" + bytes(14)) + b"\xF7")
    ok(ident and ident.model == "SMK-37 Pro" and ident.version == 16 and I.model_of("ota-SMK-37 Pro_900") == "SMK-37 Pro",
       "identity SMK-37 Pro_016 parses (model SMK-37 Pro, the space kept)")
    raw36 = package_crc("SMK-37 Pro_900", blocks=36)
    image36 = I.deinterleave(raw36)
    ok(I.block_count(raw36) == 36 and len(image36) == len(raw36) - 36 and I.files_ok(image36)
       and I.product_of(raw36) == "SMK-37 Pro_900", "36 blocks (synthetic): detected, logical image valid")
    ok(not I.files_ok(I.logical_image(raw36)), "... a fixed 20-block deinterleave of it fails the CRCs")
    for seed in range(1, 256):                  # a 20-block package with 0x7D at the 21st marker by chance
        raw = package_crc("FM-1_900", seed=seed)
        if raw[20 * I.BLK + I.KEEP] == 0x7D:
            break
    image = I.deinterleave(raw)
    ok(I.block_count(raw) > 20 and image == I.logical_image(raw) and I.files_ok(image),
       f"20 blocks with a 0x7D data byte at the 21st marker (seed {seed}): falls back to 20")
    raw = package("FM-1_900")
    ok(I.deinterleave(raw) == I.logical_image(raw), "a plain 20-block package: deinterleave == logical_image")

    p = pkgfile("smk.fwsc", package("SMK-37 Pro_900"))
    image = I.logical_image(package("SMK-37 Pro_900"))
    dev = FakeFM1(image, identity="SMK-37 Pro_016", name="SMK-37 Pro Midi", after_write="SMK-37 Pro_900",
                  loader=("ota-SMK-37 Pro_900", "Felucca Update"))
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and "SMK-37 Pro_900" in out,
       "install on an SMK-37 Pro: running -> loader -> SMK-37 Pro_900")
    dev = FakeFM1(image, identity="ota-SMK-37 Pro_900", name="Felucca Update", after_write="SMK-37 Pro_900")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.upgrades == 1, "SMK-37 Pro in Felucca's update mode -> finishes the write")
    dev = FakeFM1(image, identity="SMK-37 Pro_016", name="SMK-37 Pro Midi")
    rc, out, err = cli([pkgfile("fm1.fwsc", package("FM-1_900")), "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "FM-1 package on an SMK-37 Pro -> exit 6 (wrong model), nothing sent")
    dev = FakeFM1(image, identity="FM-1_015", name="FM-1")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "SMK-37 Pro package on an FM-1 -> exit 6, nothing sent")

    import os                                     # the stock package (outside this repo; read only)
    v016 = Path(os.environ.get("SMK_STOCK_DIR") or ROOT / "no-stock-packages") / "SMK-37_Pro_016.fwsc"
    if not v016.exists():
        print("stock SMK-37 Pro V016: skipped (SMK_STOCK_DIR not set)")
        return
    raw = v016.read_bytes()
    image = I.deinterleave(raw)
    ok(I.block_count(raw) == 36 and I.product_of(raw) == "SMK-37 Pro_016" and I.files_ok(image),
       "stock SMK-37_Pro_016.fwsc: 36 blocks, identity SMK-37 Pro_016, CRCs ok")
    product, image2 = I.load_package(str(v016), False)
    ok(product == "SMK-37 Pro_016" and image2 == image, "... accepted for the return to stock (pinned SHA-256)")


wire()
installs()
errors()
smk()
against_js()
official()
shutil.rmtree(TMP, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
