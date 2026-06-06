<p align="center">
  <img src="docs/assets/tsar-banner.png" width="750" alt="TSAR-MCP Cliff Edge AI Banner">
</p>

# TSAR-MCP: Zero-Dependency C Framework for Edge AI

**TSAR** stands for *"Tools Slightly Above the Runtime."* True to that philosophy, this project provides a hyper-lightweight, zero-dependency C framework for building Model Context Protocol (MCP) servers directly on edge, embedded, and enterprise systems. 

While the majority of the AI agent ecosystem relies on heavy runtime environments (Node.js, Python), **TSAR-MCP** is built for extreme efficiency. It compiles to a native binary, consumes virtually zero idle memory, and utilizes standard operating system streams (`stdio` over SSH) for secure, instant LLM-to-system communication.

## 🗺️ Exploring this Repository (Learning Path)

This repository is structured as a chronological masterclass in building zero-dependency C-runtime based MCP servers. Because the `main` branch contains advanced asynchronous and threading models, **we highly recommend exploring our milestone tags chronologically to understand the core architecture:**

1. **Tag: `mcp/teaching/v1.0.0`** - The zero-dependency sequential MCP baseline.
2. **Tag: `mcp/aspects/v1.1.0`** - Introduces the simple, production-ready aspect model. **Check out this teaching version to understand the essence of how aspects are built and wired.**
3. **Tag: `mcp/async/v2.0.0`** - The advanced threaded and asynchronous runtime.

Please see [ARCHITECTURE_MILESTONES.md](./ARCHITECTURE_MILESTONES.md) for detailed instructions on how to check out these historical baselines.

## The Foundation and The Future

This repository is built upon the **TSAR** C-runtime—a legacy-hardened foundation originally designed in 2012 for high-frequency SQL processing (included here as `QueryTool`). By leveraging this battle-tested bedrock, the modern MCP AI aspects inherit enterprise-grade memory management, native BNF parsing engines, and extreme execution speed, while leaving the architecture completely open for future bare-metal utilities.

## Core Architecture

TSAR-MCP leverages a battle-tested, clean-room C runtime and parsing engine to handle JSON-RPC 2.0 traffic natively:

* **Zero-Dependency Transport:** Communicates securely with LLM clients via `ssh -batch`. No sockets, no open web ports, and no custom network code—just OS-level encrypted `stdin/stdout` streams.
* **The `CommonC` Foundation:** A robust, legacy-hardened C runtime handling memory management and string operations without external library bloat.
* **Native BNF Parsing:** High-performance, compiled parsing engine (`JSONParser`) that process LLM payloads with extreme speed and safety.

## Included Examples & Roadmap

This repository includes the core framework alongside simple target-specific MCP server implementations to demonstrate the extensibility of the architecture:

* **Hello World & Port Scan (`helloWorld`, `portScan`):** Basic implementations that demonstrate how to bind standard I/O to the native JSON-RPC parsing engine, and how to interact sequentially with local network sockets.

**Enterprise Roadmap:** Because the framework compiles to a highly efficient native binary, future milestones will introduce heavy-duty enterprise aspects. This includes modules like **SAP Control (`sapControl`)**, which will allow LLMs to monitor processes and edit profiles across an SAP landscape using native commands, entirely eliminating the need for heavy SOAP gateways or third-party agents.

## Getting Started

**Building Your Own AI Tools (Aspects)**
The core of this framework is designed to be easily extensible. You build new AI capabilities by creating an **aspect**—a single `.cpp` file that defines your tool's personality (name, parameters, and execution logic) while plugging into the underlying native engine. The core framework handles all JSON-RPC parsing, memory management, and standard I/O automatically. 

To learn how to quickly scaffold, build, and test your own custom tools, please read the **[MCPServer Aspect Guide](./mcp/doc/MCPServer_AspectGuide.md)**.

**Building the MCP Framework:**
Navigate to the `mcp/` directory. The framework uses an out-of-source build system, placing compiled binaries into the `build/Release` or `build/Debug` directories.

**Linux / macOS (GNU Make):**
```bash
cd mcp
make                 # Builds all aspects (Release mode)
make CFG=Debug       # Builds all aspects (Debug mode)
make helloWorld      # Builds only the helloWorld aspect
make cleanall        # Removes the entire build directory
```

**Windows**
```bash
cd mcp
nmake -f Makefile.nmake                 # Builds all aspects (Release mode)
nmake -f Makefile.nmake CFG=Debug       # Builds all aspects (Debug mode)
nmake -f Makefile.nmake helloWorld      # Builds only the helloWorld aspect
nmake -f Makefile.nmake cleanall        # Removes the entire build directory
```

## License & Authorship

Copyright (c) 2026 International Business Machines
Architected and Authored by Eric Kass
Licensed under the [MIT License](LICENSE).