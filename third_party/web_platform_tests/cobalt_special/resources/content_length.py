# Lint as: python3
"""
Helper script to send XHR response with specified content length.
"""

def main(request, response):
  headers = []

  content_length = request.GET.first("content_length")
  headers.append(("Content-Length", content_length));

  body = "this is body"

  return headers, body
