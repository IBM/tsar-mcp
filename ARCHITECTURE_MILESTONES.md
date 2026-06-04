# Architecture Milestones & Repository Timeline

Because the TSAR ecosystem contains a legacy database utility alongside a modern Model Context Protocol (MCP) framework, we use hierarchical, slash-delimited Git Tags to isolate domains and provide a chronological learning path.

If you are trying to understand the architecture, do not start on the `main` branch. The `main` branch contains highly optimized, asynchronous code. Instead, clone the specific milestone tags below to learn the architecture step-by-step.

## 🌐 Model Context Protocol (`mcp/`)

These tags trace the evolution of the TSAR-MCP framework from a raw sequential baseline to a high-performance asynchronous runtime. 

| Tag | Architectural Milestone |
| :--- | :--- |
| **`mcp/teaching/v1.0.0`** | **The Sequential Baseline:** Start here. Introduces zero-dependency standard I/O bindings and the core JSON-RPC 2.0 parsing engine without any complex threading overhead. |
| **`mcp/aspects/v1.1.0`** | **The Extensibility Model:** Introduces "Aspects," demonstrating the simple, production-ready pattern for defining and loading new MCP servers into the framework. |
| **`mcp/async/v2.0.0`** | **The Modern Runtime:** Introduces advanced threading and asynchronous client communication for high-performance edge execution. |

### How to Clone an MCP Milestone
To keep your workspace clean and avoid downloading the entire repository history, use a "shallow clone" to pull down only the exact files for the milestone you want to study:

```bash
# Example: Cloning the Teaching Baseline
git clone --branch refs/tags/mcp/teaching/v1.0.0 --depth 1 https://github.com/IBM/tsar-mcp.git
```
*(Note: This places your local repository in a "detached HEAD" state, which is perfectly safe and expected for reading and compiling historical code).*

---

## 💾 Database Utilities (`db/`)

The TSAR repository also preserves the legacy high-frequency SQL processing tools that form the battle-tested bedrock of the `base/CommonC` libraries.

| Tag | Architectural Milestone |
| :--- | :--- |
| **`db/querytool/v1.0.0`** | **Legacy Provenance:** The initial clean-room import of the original 2012 SourceForge QueryTool and core C-runtime. |

*(Note: Future TSAR domains, such as Networking, will be tracked here under their respective `net/` namespaces).*