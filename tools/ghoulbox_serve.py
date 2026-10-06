#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GHOULBOX: build the installer site for build/felucca.fwsc under a name of its own (the GHOULBOX version,
# so a browser can never install a cached older package) and serve it on 127.0.0.1:8000 with no caching.
#   python3 tools/ghoulbox_serve.py
import functools
import http.server
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        super().end_headers()


def main():
    ver = re.search(r'#define GHOULBOX_VERSION "([^"]+)"', (ROOT / "firmware/src/core.h").read_text())[1]
    site = Path(tempfile.mkdtemp(prefix="ghoulbox-site-"))
    subprocess.run([sys.executable, ROOT / "web/make_site.py", ROOT / "build/felucca.fwsc", f"gb{ver}", site], check=True)
    print(f"GHOULBOX {ver}: http://localhost:8000/webapp/installer/  (Ctrl+C to stop)")
    http.server.ThreadingHTTPServer(("127.0.0.1", 8000), functools.partial(NoCache, directory=str(site))).serve_forever()


if __name__ == "__main__":
    main()
