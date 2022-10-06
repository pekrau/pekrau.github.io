"Serve the local docs."

import http.server
import socketserver
import functools

PORT = 8000

handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory="docs")

with socketserver.TCPServer(("", PORT), handler) as httpd:
    print(f"serving at http://0.0.0.0:{PORT}/")
    httpd.serve_forever()
