> 🚧 **WORK IN PROGRESS:** This repository is currently undergoing initial construction and migration. The core architecture is defined below, and the foundational C runtime and parsing engines are actively being staged. 

# TSAR-MCP: Zero-Dependency C Framework for Edge AI

A hyper-lightweight, zero-dependency C framework for building Model Context Protocol (MCP) servers on edge, embedded, and enterprise systems. 

While the majority of the AI agent ecosystem relies on heavy runtime environments (Node.js, Python), **TSAR-MCP** is built for extreme efficiency. It compiles to a native binary, consumes virtually zero idle memory, and utilizes standard operating system streams (`stdio` over SSH) for secure, instant LLM-to-system communication.

## Core Architecture

TSAR-MCP leverages a battle-tested, clean-room C runtime and parsing engine to handle JSON-RPC 2.0 traffic natively:

* **Zero-Dependency Transport:** Communicates securely with LLM clients via `ssh -batch`. No sockets, no open web ports, and no custom network code—just OS-level encrypted `stdin/stdout` streams.
* **The `CommonC` Foundation:** A robust, legacy-hardened C runtime handling memory management and string operations without external library bloat.
* **Native BNF Parsing:** High-performance, compiled parsing engines (`JSONParser`, `MLparser`) that process LLM payloads with extreme speed and safety.

## Included Servers

This repository includes the core framework alongside target-specific MCP server implementations:

* **SAP Control (`sapControl`):** An enterprise-grade MCP server that wraps native SAP commands. It allows LLMs to monitor processes, edit profiles, and execute file system operations across an SAP landscape without deploying heavy SOAP gateways or third-party runtime agents.

## Getting Started

*(Placeholder for build instructions: e.g., `make all`)*

## License & Authorship

Copyright (c) 2026 International Business Machines
Architected and Authored by Eric Kass
Licensed under the [MIT License](LICENSE).
