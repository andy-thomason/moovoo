import os
import sys
import ssl
import urllib
import http.server

import moovoo

#inst = moovoo.Instance()

with open("1.jpg", "rb") as f1:
  j1 = f1.read()

with open("2.jpg", "rb") as f1:
  j2 = f1.read()

class moovoo_server(http.server.HTTPServer):
  def __init__(self, addr):
    http.server.HTTPServer.__init__(self, addr, handler)

class moovoo_handler(http.server.BaseHTTPRequestHandler):
  def do_GET(s):
    url = urllib.parse.urlparse(s.path)
    query = urllib.parse.parse_qs(url.query)
    if s.path == '/':
      s.send_response(200)
      s.send_header("Content-type", "text/html")
      s.end_headers()
      s.wfile.write(b"<html><head><title>Title goes here.</title></head>")
      s.wfile.write(b"<body><img src='1.jpg'></body></html>")
    else:
      s.send_response(200)
      s.send_header("Connection", "close")
      s.send_header("Max-Age", "0")
      s.send_header("Expires", "0")
      s.send_header("Cache-Control", "no-cache, private")
      s.send_header("Pragma", "no-cache")
      s.send_header("Content-Type", "multipart/x-mixed-replace; boundary=--BoundaryString");
      s.end_headers()
      while True:
        s.wfile.write(b"--BoundaryString\r\nContent-type: image/jpg\r\nContent-Length: %d\r\n\r\n" % len(j1))
        s.wfile.write(j1)
        s.wfile.write(b"--BoundaryString\r\nContent-type: image/jpg\r\nContent-Length: %d\r\n\r\n" % len(j2))
        s.wfile.write(j2)

def main(argv):
  ip = "0.0.0.0"
  port = 443
  httpd = http.server.HTTPServer((ip, port), moovoo_handler)
  httpd.socket = ssl.wrap_socket (httpd.socket, certfile='cert.pem', server_side=True)
  while True:
    httpd.handle_request()

if __name__ == "__main__":
  try:
    main(sys.argv)
  except KeyboardInterrupt as k:
    print("Keyboard interrupt")




