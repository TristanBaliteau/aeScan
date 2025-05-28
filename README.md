# aeScan

> **Adaptive Efficient Scanner** — A fast and simple network scanner in C that detects active hosts on your local network and scans open ports.

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

## Installation

Clone the repository:
```bash
git clone https://github.com/TristanBaliteau/aeScan.git
```

Compile with:
```bash
make
```

Install system-wide so you can run aescan from anywhere:
```bash
sudo make install
```

Remove the installed executable from the system:
```bash
sudo make uninstall
```

Remove compiled files:
```bash
make clean
```

