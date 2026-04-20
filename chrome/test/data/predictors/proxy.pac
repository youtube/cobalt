function FindProxyForURL(url, host)
{
  if (shExpMatch(host, "test.com|foo.com|baz.com|bar.com"))
  {
    return "PROXY 127.0.0.1:REPLACE_WITH_PORT";
  }

  // All other requests don't need a proxy:
  return "DIRECT";
}
