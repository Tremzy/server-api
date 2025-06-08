
# C++ Minimal HTTP API Server

A lightweight, low-level HTTP server written in C++ using raw socket programming.  
Designed for learning, experimentation, and showcasing low-resource server design without any frameworks.

## 🌐 Features

- Accepts and processes HTTP GET requests
- Serves data based on path
- IP-based **connection rate limiting** to mitigate abuse (per-IP burst control)
- Fully manual request parsing using basic `read()`/`send()`
- Manual (low-level) struct management using pointers

## ⚙️ Rate Limiting Logic

Each client is tracked by IP, and a maximum of 10 connections is allowed in a short configurable window (`MINSECPERTENCONS`).  
New connections that violate this window are deferred (connection is silently closed), and if necessary, banned, making the server more resistant to spam/floods.

## 🧪 Example Usage

Start the server:
```bash
./api-server
```

Send a request:
```bash
curl http://localhost:8080/
curl http://localhost:8080/api/utc
```

## 📁 Project Structure

```
src/
├── server.cpp         # Main server logic
├── htdocs/index.html  # Static HTML content
```

## 📦 Dependencies

- C++20-compliant compiler (`g++`, `clang++`, or MSVC)
- POSIX-compatible system (Linux, macOS, WSL recommended)
- No external libraries required

## 🛠️ Compiling

Run the following in the root directory:

```bash
g++ -std=c++20 -o api-server src/server.cpp
```

Then start the server:

```bash
./api-server
```

## 🧠 Why

This project was built to understand:
- How HTTP actually works under the hood
- How to implement real-world protections (like rate limiting) manually
- How to structure a minimal API server in pure C++
