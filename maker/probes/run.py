#!/usr/bin/env python3
"""Lance une page de sonde dans Chrome et recupere son rapport JSON.

  python maker/probes/run.py --page step7.html --query "pages=1&smooth=7"
  python maker/probes/run.py --page coop_coep.html --coop-coep
  python maker/probes/run.py --page coop_coep.html          (controle negatif)

Le serveur (serve_probe.py) sert maker/ et attend un POST /result. Chrome est
lance en headless ; --headed le rend visible, ce qui est la seule facon
d'obtenir un renderer GPU reel plutot que SwiftShader (reserve nodal.md 9.3).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time

import serve_probe

CHROME_CANDIDATES = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
]


def find_browser():
    for path in CHROME_CANDIDATES:
        if os.path.exists(path):
            return path
    found = shutil.which("chrome") or shutil.which("chromium")
    if found:
        return found
    raise SystemExit("aucun navigateur Chromium trouve")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--page", default="step7.html")
    ap.add_argument("--query", default="")
    ap.add_argument("--port", type=int, default=8123)
    ap.add_argument("--coop-coep", action="store_true")
    ap.add_argument("--headed", action="store_true")
    ap.add_argument("--timeout", type=int, default=900)
    ap.add_argument("--out", default=os.path.join(here, "last_result.json"))
    args = ap.parse_args()

    done = threading.Event()
    httpd = serve_probe.serve(
        directory=os.path.dirname(here),
        port=args.port,
        bind="127.0.0.1",
        coop_coep=args.coop_coep,
        out_path=args.out,
        done=done,
    )

    url = f"http://127.0.0.1:{args.port}/probes/{args.page}"
    if args.query:
        url += "?" + args.query

    profile = tempfile.mkdtemp(prefix="cg-probe-")
    flags = [
        find_browser(),
        f"--user-data-dir={profile}",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-extensions",
        # WebGL logiciel : Chrome l'exige explicitement depuis qu'il refuse
        # SwiftShader par defaut. Sans lui, getContext("webgl2") rend null en
        # headless et la sonde 7.3 echouerait pour une raison qui n'est pas la
        # sienne.
        "--enable-unsafe-swiftshader",
        # 4 Gio : la mesure d'empreinte doit pouvoir depasser le defaut de
        # Chrome sans que la limite du navigateur soit ce qu'on mesure.
        "--js-flags=--max-old-space-size=4096",
    ]
    if not args.headed:
        flags.append("--headless=new")
    flags.append(url)

    if os.path.exists(args.out):
        os.remove(args.out)

    print("sonde :", url, "| COOP/COEP:", args.coop_coep, "| headless:", not args.headed)
    browser = subprocess.Popen(flags, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ok = done.wait(args.timeout)
    browser.terminate()
    httpd.shutdown()
    shutil.rmtree(profile, ignore_errors=True)

    if not ok:
        print(f"AUCUN RESULTAT apres {args.timeout} s", file=sys.stderr)
        return 2

    with open(args.out, encoding="utf-8") as f:
        report = json.load(f)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
