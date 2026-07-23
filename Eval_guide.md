# Webserv 42 Evaluation Guide

All commands assume you are in the project root: `cd ~/webserv`

---

## 1. STEP 0: SETUP

### Terminal 1 (build + server)

```bash
# Clean rebuild
make re
```
**Expected:** Compiles with `c++ -Wall -Wextra -Werror -std=c++98`, produces `webserv` binary. No errors.

```bash
# Verify no relink
make
```
**Expected:** `make: 'webserv' is up to date.`

```bash
# Ensure CGI scripts are executable
chmod +x cgi-bin/*.py cgi-bin/*.sh cgi-bin/*.pl
```

```bash
# Create upload directory if missing
mkdir -p www/upload
```

```bash
# Install siege (if not installed)
sudo apt install -y siege
```

```bash
# Start the server (keep this terminal running)
./webserv config/default.conf
```
**Expected:** Server starts, listening on ports 8080 and 8081. No crash.

---

## 2. CHECK THE CODE AND ASK QUESTIONS

### Terminal 1

```bash
# Verify C++98 standard, Wall Wextra Werror
grep -E 'CXXFLAGS|CXX\b' Makefile
```
**Expected:** `CXX = c++` and `CXXFLAGS = -Wall -Wextra -Werror -std=c++98`

```bash
# Verify no relink after build
make
```
**Expected:** `make: 'webserv' is up to date.`

```bash
# Check source files exist
ls src/*.cpp include/*.hpp
```
**Expected:** Lists all source and header files (Config, Server, Client, Request, Response, Router, CGI, Utils, main).

---

## 3. CONFIGURATION

### Terminal 2 (all curl tests while server runs in Terminal 1)

### 3.1 Multiple ports

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200`

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8081/
```
**Expected:** `200`

### 3.2 Hostname resolution (use multi_server.conf for this test)

Stop server in Terminal 1, restart with:
```bash
./webserv config/multi_server.conf
```

```bash
# Virtual host: example.com resolves to different content
curl --resolve example.com:8080:127.0.0.1 -s http://example.com:8080/
```
**Expected:** Returns content from `www/errors/` root (the 404.html page as index). Different from localhost:8080.

```bash
# Default host
curl -s http://localhost:8080/ | head -5
```
**Expected:** Returns content from `www/index.html`.

Stop server, restart with default config:
```bash
./webserv config/default.conf
```

### 3.3 Custom error pages

```bash
curl -s http://localhost:8080/nonexistent_page
```
**Expected:** Returns custom 404 error page content (from `www/errors/404.html`).

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/nonexistent_page
```
**Expected:** `404`

### 3.4 Client body size limit

```bash
# Port 8081 has 1M limit - send >1M
python3 -c "print('x' * 2000000)" | curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: text/plain" -d @- http://localhost:8081/
```
**Expected:** `413` (Payload Too Large)

```bash
# Port 8080 has 10M limit - 2M should be fine
python3 -c "print('x' * 2000000)" | curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: text/plain" -d @- http://localhost:8080/
```
**Expected:** `200` (or any non-413 response)

### 3.5 Routes and methods per location

```bash
# Port 8081 only allows GET - DELETE should fail
curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8081/
```
**Expected:** `405` (Method Not Allowed)

```bash
# Port 8081 only allows GET - POST should fail
curl -s -o /dev/null -w "%{http_code}" -X POST -d "test" http://localhost:8081/
```
**Expected:** `405`

### 3.6 Default file (index)

```bash
# / should serve index.html
curl -s http://localhost:8080/ | grep -i "<title>"
```
**Expected:** Contains the title from `www/index.html`.

### 3.7 Directory listing

```bash
# /files has autoindex on
curl -s http://localhost:8080/files/
```
**Expected:** HTML directory listing showing `hello.txt`, `readme.txt`, `sample.txt`.

```bash
# / has autoindex off - should NOT show directory listing
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/src/
```
**Expected:** `404` (no directory listing, no index file found).

---

## 4. BASIC CHECKS

### Terminal 2

### 4.1 GET request

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200`

```bash
curl -s http://localhost:8080/style.css | head -3
```
**Expected:** CSS content returned.

### 4.2 POST request

```bash
curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: text/plain" -d "hello" http://localhost:8080/
```
**Expected:** `200`

### 4.3 DELETE request

```bash
# Upload a file first, then delete it
curl -s -X POST -F "file=@Makefile" http://localhost:8080/upload
curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8080/upload/Makefile
```
**Expected:** `200` (file deleted)

```bash
# Delete non-existent file
curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8080/upload/does_not_exist
```
**Expected:** `404`

### 4.4 Unknown method

```bash
curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8080/
```
**Expected:** `501` (Not Implemented) or `405`

```bash
curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8080/
```
**Expected:** `501` (Not Implemented) or `405`

### 4.5 Status codes verification

```bash
# 200 OK
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200`

```bash
# 301 Redirect
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/old-page
```
**Expected:** `301`

```bash
# 404 Not Found
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/does_not_exist
```
**Expected:** `404`

```bash
# 405 Method Not Allowed
curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8081/
```
**Expected:** `405`

### 4.6 Upload and retrieve

```bash
# Upload
curl -s -X POST -F "file=@Makefile" http://localhost:8080/upload
```
**Expected:** `200` or success message confirming upload.

```bash
# Retrieve uploaded file
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/upload/Makefile
```
**Expected:** `200`

```bash
# Verify content matches
diff <(curl -s http://localhost:8080/upload/Makefile) Makefile
```
**Expected:** No output (files are identical).

```bash
# Cleanup
curl -s -X DELETE http://localhost:8080/upload/Makefile
```
**Expected:** `200`

---

## 5. CHECK WITH BROWSER

### Browser (open while server runs in Terminal 1)

```
Open: http://localhost:8080/
```
**Expected:** Static HTML page renders. Open DevTools (F12) > Network tab.

### 5.1 Headers in Network tab

```
Reload http://localhost:8080/ with Network tab open.
Click the request, check Response Headers.
```
**Expected:** `Content-Type: text/html`, `Content-Length` present, valid HTTP status line.

### 5.2 Static site works

```
Navigate to: http://localhost:8080/style.css
```
**Expected:** CSS file displayed. Content-Type should be `text/css`.

### 5.3 Wrong URL

```
Navigate to: http://localhost:8080/this_page_does_not_exist
```
**Expected:** Custom 404 error page displayed.

### 5.4 Directory listing

```
Navigate to: http://localhost:8080/files/
```
**Expected:** Directory listing with clickable links to `hello.txt`, `readme.txt`, `sample.txt`.

### 5.5 Redirect

```
Navigate to: http://localhost:8080/old-page
```
**Expected:** Browser redirects to `http://localhost:8080/`. Check Network tab shows 301 then 200.

### Verify with curl (Terminal 2)

```bash
# Check headers
curl -I http://localhost:8080/
```
**Expected:** HTTP/1.1 200 OK, Content-Type: text/html, Content-Length present.

```bash
# Redirect headers
curl -I http://localhost:8080/old-page
```
**Expected:** HTTP/1.1 301, Location: /

---

## 6. PORT ISSUES

### 6.1 Multiple ports in browser

```
Open Tab 1: http://localhost:8080/
Open Tab 2: http://localhost:8081/
```
**Expected:** Both pages load. Port 8081 serves content (GET only).

### Terminal 2

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
echo ""
curl -s -o /dev/null -w "%{http_code}" http://localhost:8081/
```
**Expected:** Both return `200`.

### 6.2 Same port configured twice (duplicate_port.conf)

Stop server in Terminal 1, then:

```bash
./webserv config/duplicate_port.conf
```
**Expected:** Server either starts with a warning about duplicate port, or handles it gracefully (does not crash). Only one socket bound to 8080.

### Terminal 2 (if server started)

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200` (server responds normally despite duplicate config).

### 6.3 Two instances on same port

Stop server. Start first instance:

```bash
# Terminal 1
./webserv config/default.conf
```

```bash
# Terminal 3: try starting second instance on same port
./webserv config/default.conf
```
**Expected:** Second instance fails with "Address already in use" or similar error. Does NOT crash the first instance.

### Terminal 2 (verify first instance still works)

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200` (first instance unaffected).

Restart server with default config for remaining tests:
```bash
# Terminal 1
./webserv config/default.conf
```

---

## 7. SIEGE AND STRESS TEST

### Terminal 2

### 7.1 Basic siege test

```bash
siege -b -c 10 -t 30S http://127.0.0.1:8080/
```
**Expected:** Availability > 99.5%. No failed transactions (or very few). Server does not crash.
（http://localhost:8080/ -> ::1 == IPv6）
(http://127.0.0.1:8080 -> == IPv4)

### 7.2 Higher concurrency

```bash
siege -b -c 50 -t 10S http://127.0.0.1:8080/
```
**Expected:** Availability > 99.5%. Server stays responsive.

### 7.3 Memory leak check

```bash
# Terminal 3: monitor memory while siege runs
# Get PID
PID=$(pgrep webserv)
echo "PID: $PID"

# Check memory before
ps -o pid,rss,vsz -p $PID
```

```bash
# Terminal 2: run siege
siege -b -c 20 -t 30S http://127.0.0.1:8080/
```

```bash
# Terminal 3: check memory after
ps -o pid,rss,vsz -p $(pgrep webserv)
```
**Expected:** RSS (resident memory) should not grow significantly after siege. No unbounded memory growth.

### 7.4 Valgrind leak check (optional, run separately)

```bash
# Stop server first, then:
make valgrind
# In another terminal, run a few requests, then stop server with Ctrl+C
```
**Expected:** No definitely lost blocks. Fd tracking shows no leaked file descriptors.

### 7.5 No hanging connections

```bash
# After siege, server should still respond immediately
curl -s -o /dev/null -w "%{http_code} %{time_total}s\n" http://localhost:8080/
```
**Expected:** `200` with response time < 1 second. No hanging.

### 7.6 Siege indefinitely (run until evaluator stops it with Ctrl+C)

```bash
siege -b -c 10 http://127.0.0.1:8080/
```
**Expected:** Server handles continuous load. Stop with Ctrl+C. Server still responds after:

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/
```
**Expected:** `200`

---

## 8. BONUS: CGI

### Terminal 2

### 8.1 Python CGI

```bash
curl -s http://localhost:8080/cgi-bin/hello.py
```
**Expected:** HTML page with "Hello from CGI!", shows `Request Method: GET`.

```bash
curl -s "http://localhost:8080/cgi-bin/hello.py?name=42&test=true"
```
**Expected:** HTML page, `Query String: name=42&test=true`.

```bash
curl -s -X POST -d "name=42" http://localhost:8080/cgi-bin/form.py
```
**Expected:** HTML page with "Hello, 42!" and `Received POST data: name=42`.

```bash
curl -s http://localhost:8080/cgi-bin/env.py | grep -i "REQUEST_METHOD\|SERVER_SOFTWARE\|QUERY_STRING"
```
**Expected:** Table rows showing CGI environment variables are set correctly.

### 8.2 Bash CGI

```bash
curl -s http://localhost:8080/cgi-sh/hello.sh
```
**Expected:** HTML page with "Hello from Bash CGI!", shows Request Method, date output.

### 8.3 Perl CGI

```bash
curl -s http://localhost:8080/cgi-pl/hello.pl
```
**Expected:** HTML page with "Hello from Perl CGI!", shows Request Method and Server Software.

### 8.4 CGI with query string

```bash
curl -s "http://localhost:8080/cgi-sh/hello.sh?foo=bar"
```
**Expected:** `Query String: foo=bar` visible in output.

```bash
curl -s "http://localhost:8080/cgi-pl/hello.pl?lang=perl"
```
**Expected:** `Query String: lang=perl` visible in output.

### 8.5 CGI error handling

```bash
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/cgi-bin/nonexistent.py
```
**Expected:** `502 or 512 or 404`

## 9. 42 TESTER 

Before entering the resource / quick reference section, verify that the tester works correctly.

### Terminal 1
```bash
./webserv config/tester.conf
```

### Terminal 2
```bash
chmod +x /Youpibanane/cgi_tester tester

./tester http://localhost:8080
```
**Expected:**  
Output contains `success`.


---

## Quick Reference: Config Files

| Config | Ports | Use for |
|--------|-------|---------|
| `config/default.conf` | 8080, 8081 | Main tests (sections 3-5, 7-8) |
| `config/multi_server.conf` | 8080, 8081 | Virtual hosting test (section 3.2) |
| `config/duplicate_port.conf` | 8080, 8080 | Duplicate port test (section 6.2) |

## Quick Reference: Expected Status Codes

| Request | Code |
|---------|------|
| `GET /` | 200 |
| `GET /style.css` | 200 |
| `GET /files/` | 200 (directory listing) |
| `GET /nonexistent` | 404 |
| `GET /old-page` | 301 |
| `POST /upload` (with file) | 200 |
| `DELETE /upload/file` | 200 |
| `DELETE /upload/nope` | 404 |
| `DELETE /` on port 8081 | 405 |
| `PUT /` | 501 |
| `PATCH /` | 501 |
| Oversized body on 8081 | 413 |
| `GET /cgi-bin/hello.py` | 200 |
