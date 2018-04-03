def main(request, response):
    headers = [("Content-Type", "text/plain")]


    headers.append(("Access-Control-Allow-Origin", "*"))
    allowed_headers = "x-print"
    if "Access-Control-Request-Headers" in request.headers:
        allowed_headers = request.headers.get(
            "Access-Control-Request-Headers", "")
    headers.append(("Access-Control-Allow-Headers", allowed_headers))

    if "check" in request.GET:
        token = request.GET.first("token")
        value = request.server.stash.take(token)
        if value == None:
            body = "0"
        else:
            if request.GET.first("check", None) == "keep":
                request.server.stash.put(token, value)
            body = "1"

        return headers, body

    if request.method == "OPTIONS":
        if not "Access-Control-Request-Method" in request.headers:
            response.set_error(400, "No Access-Control-Request-Method header")
            return "ERROR: No access-control-request-method in preflight!"

        headers.append(("Access-Control-Allow-Methods",
                        request.headers['Access-Control-Request-Method']))

        if "token" in request.GET:
            if allowed_headers.find("first_request") != -1:
                # Put a cache entry that lasts longer for the first request.
                headers.append(("Access-Control-Max-Age", "3"))
            else:
                headers.append(("Access-Control-Max-Age", "1"))
            token = request.GET.first("token")
            value = request.server.stash.take(token)
            request.server.stash.put(token, 1)

    body = request.headers.get("x-print", "NO")

    return headers, body
