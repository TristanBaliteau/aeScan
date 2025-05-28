# aeScan

> **Adaptive Network Scanner** — A fast and simple network scanner in C that detects active hosts on your local network and scans open ports.

---

## Features

- Automatically detects your local IPv4 network and subnet mask
- Pings all hosts on the local network to find active devices
- Resolves hostnames for detected IP addresses
- Performs multi-threaded port scanning (1-65535) on selected hosts
- Colorful console output for better readability
- Lightweight and written in C with pthreads for concurrency

---

## Requirements

- Linux or Unix-like system
- GCC compiler
- POSIX threads (`pthread`)
- Network tools (e.g., `ping`) available in PATH

---

## Build

Clone the repository and compile with:

```bash
make
