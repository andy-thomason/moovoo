import os
import io
import sys
import ssl
import urllib
from PIL import Image
import http.server

import moovoo

SIZE=(1200, 800)



class moovoo_server(http.server.HTTPServer):
  def __init__(s, addr, handler):
    http.server.HTTPServer.__init__(s, addr, handler)
    s.ctxt = moovoo.Context()
    s.modelBytes = open("../molecules/2tgt.cif", "rb").read()
    s.model = moovoo.Model(s.ctxt, s.modelBytes)
    s.view = moovoo.View(s.ctxt, s.model, SIZE[0], SIZE[1])

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
        data = s.server.view.render(s.server.ctxt, s.server.model)
        pilimg = Image.frombuffer("RGBA", SIZE, data, "raw", "RGBA", 0, 1)
        stream = io.BytesIO()
        pilimg.save(stream, "PNG")
        b = stream.getbuffer()
        s.wfile.write(b"--BoundaryString\r\nContent-type: image/png\r\nContent-Length: %d\r\n\r\n" % len(b))
        s.wfile.write(b)

def main(argv):
  ip = "0.0.0.0"
  port = 8000
  httpd = moovoo_server((ip, port), moovoo_handler)
  #httpd.socket = ssl.wrap_socket (httpd.socket, certfile='cert.pem', server_side=True)
  while True:
    httpd.handle_request()

if __name__ == "__main__":
  try:
    main(sys.argv)
  except KeyboardInterrupt as k:
    print("Keyboard interrupt")




