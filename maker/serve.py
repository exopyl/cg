#!/usr/bin/env python3
"""Serveur de developpement pour maker/web.

`python -m http.server` nu n'envoie AUCUN en-tete Cache-Control : avec seulement
Last-Modified, les navigateurs appliquent une fraicheur heuristique aux .js et
.wasm. Resultat : apres un `build.ps1`, index.html se recharge mais maker.js /
maker.wasm restent ceux de la build precedente -> un binding Embind recent est
`undefined` et la page semble cassee sans raison visible.

On force donc `Cache-Control: no-store` sur tout. Cout nul en local, et plus
jamais de "j'ai reconstruit mais rien n'a change".
"""
import argparse
import functools
import http.server


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    # Un log par requete suffit ; pas de bruit sur les 404 de favicon.
    def log_message(self, fmt, *args):
        if "favicon" in self.path:
            return
        super().log_message(fmt, *args)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--directory", required=True)
    args = ap.parse_args()

    handler = functools.partial(NoCacheHandler, directory=args.directory)
    with http.server.ThreadingHTTPServer((args.bind, args.port), handler) as httpd:
        print(f"http://{args.bind}:{args.port}/  ({args.directory})  [no-store]")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
