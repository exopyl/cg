#!/usr/bin/env python3
"""Serveur des sondes de maker/ -- CONSERVE, et c'est sa raison d'etre.

Les mesures COOP/COEP du 2026-08-14 (nodal.md sec.9) avaient ete prises par des
scripts jetables qui n'ont pas ete gardes : les chiffres ont survecu, pas
l'instrument qui les produit. C'est la reserve R-audit. Ce fichier est la
reponse -- il rejoue les memes sondes, et il reste dans le depot.

Deux differences avec maker/serve.py, et elles sont les seules :

  --coop-coep  ajoute Cross-Origin-Opener-Policy: same-origin et
               Cross-Origin-Embedder-Policy: require-corp. Sans le drapeau, le
               serveur ne les pose pas -- c'est le CONTROLE NEGATIF, et il
               compte autant que la mesure positive.

  POST /result la page depose son resultat JSON ; le serveur l'ecrit dans
               --out et s'arrete. Une sonde de navigateur qui n'a pas de canal
               de retour se lit a l'oeil, donc ne se rejoue pas.

La racine servie est maker/ : /web/... est le produit, /probes/... les sondes.
"""
import argparse
import functools
import http.server
import json
import os
import sys
import threading


class ProbeHandler(http.server.SimpleHTTPRequestHandler):
    coop_coep = False
    out_path = None
    done = None

    def end_headers(self):
        # Meme motif que maker/serve.py : sans no-store, un maker.wasm
        # reconstruit reste celui du cache et la sonde mesure l'artefact
        # precedent.
        self.send_header("Cache-Control", "no-store, must-revalidate")
        if ProbeHandler.coop_coep:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def do_POST(self):
        if self.path != "/result":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        self.send_response(204)
        self.end_headers()
        if ProbeHandler.out_path:
            with open(ProbeHandler.out_path, "wb") as f:
                f.write(body)
        else:
            sys.stdout.write(body.decode("utf-8", "replace") + "\n")
        try:
            json.loads(body)
        except Exception:
            pass
        if ProbeHandler.done is not None:
            ProbeHandler.done.set()

    def log_message(self, fmt, *args):
        if "favicon" in self.path:
            return
        super().log_message(fmt, *args)


def serve(directory, port, bind, coop_coep, out_path, done):
    ProbeHandler.coop_coep = coop_coep
    ProbeHandler.out_path = out_path
    ProbeHandler.done = done
    handler = functools.partial(ProbeHandler, directory=directory)
    httpd = http.server.ThreadingHTTPServer((bind, port), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    return httpd


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8123)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--directory", default=os.path.dirname(here))
    ap.add_argument("--coop-coep", action="store_true")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    done = threading.Event()
    serve(args.directory, args.port, args.bind, args.coop_coep, args.out, done)
    flags = "COOP/COEP" if args.coop_coep else "sans en-tetes (controle negatif)"
    print(f"http://{args.bind}:{args.port}/  ({args.directory})  [{flags}]")
    try:
        done.wait()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
