# TSAR-MCP: Client Wiring & Transport Guide

The TSAR-MCP framework uses a strict **Zero-Dependency Standard I/O** transport model. This means the server communicates exclusively over `stdin` and `stdout`.

Because it does not bind to network sockets or require HTTP daemons, you can connect an AI client to a TSAR-MCP server locally on the same machine, or securely across the world over an SSH tunnel, using the exact same JSON configuration.

Below are examples of how to configure your MCP client (such as VSCode or Claude Desktop) for different connection types.

## 1. Local Execution (Direct stdio)

When the MCP server executable resides on the same machine as the AI client, the configuration simply points directly to the binary. The client will launch the executable as a child process and bind directly to its I/O streams.

**Example: `mcp.json**`

```json
{
  "mcpServers": {
    "helloWorld_local": {
      "type": "stdio",
      "command": "D:\\tsar-mcp\\mcp\\build\\Release\\MCPServer_helloWorld.exe",
      "args": [
        "-debug"
      ]
    }
  }
}

```

## 2. Remote Execution (OpenSSH)

Because TSAR-MCP relies entirely on standard streams, you can run the server on a remote Linux edge device or enterprise server by simply wrapping the call in an SSH command.

The AI client runs `ssh.exe` locally. The SSH client securely connects to the remote machine, executes the TSAR-MCP binary, and pipes the remote `stdin/stdout` directly back to the local AI client.

*Requirement: You must have SSH key-based authentication configured. If SSH prompts for a password, it will corrupt the JSON-RPC stream.*

**Example: `mcp.json**`

```json
{
  "mcpServers": {
    "helloWorld_remote": {
      "type": "stdio",
      "command": "C:\\WINDOWS\\System32\\OpenSSH\\ssh.exe",
      "args": [
        "eric@taurus",
        "tsar-mcp/mcp/build/Release/MCPServer_helloWorld",
        "-debug",
        "-trace"
      ]
    }
  }
}

```

## 3. Remote Execution (PuTTY / Plink)

If you are on a Windows environment and prefer to use the PuTTY suite for SSH management, you can use `PLINK.EXE`.

When using Plink, include the `-batch` flag. This disables all interactive prompts (like host key warnings), ensuring the standard output stream remains pure JSON-RPC data. Do not allocate a pseudo-terminal (`-t`) when wiring to an AI client (see Troubleshooting).

**Example: `mcp.json**`

```json
{
  "mcpServers": {
    "helloWorld_plink": {
      "type": "stdio",
      "command": "C:\\Program Files\\PuTTY\\PLINK.EXE",
      "args": [
        "-batch",
        "eric@taurus",
        "tsar-mcp/mcp/build/Release/MCPServer_helloWorld"
      ]
    }
  }
}

```

## 4. Local Execution with Universal Sampling (`env` Configuration)

Servers that use the hybrid blocking sampler (`MCPSyncSamplerEx`) take their configuration from environment variables. The example below arms **both** fallbacks so the whole chain is visible; the two groups are independent, and configuring only one is normal.

**Example: `mcp.json`**

```json
{
  "mcpServers": {
    "spellCheck_local": {
      "type": "stdio",
      "command": "D:\\tsar-mcp\\mcp\\build\\Release\\MCPServer_spellCheck.exe",
      "args": [
        "-debug"
      ],
      "env": {
        "MCPSample_ACP_CMD": "node \"%APPDATA%\\npm\\node_modules\\@ibm\\bob\\dist\\bob.js\" acp --trust",
        "MCPSample_ACP_CWD": "~",
        "BOB_API_KEY": "YOUR_BOB_KEY",

        "MCPSample_AI_URL": "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent",
        "MCPSample_AI_MODEL": "gemini-2.5-flash",
        "MCPSample_AI_KEY": "YOUR_GEMINI_KEY"
      }
    }
  }
}
```
On Linux the same two ACP settings become:

```json
        "MCPSample_ACP_CMD": "/usr/local/bin/bob acp --trust",
        "MCPSample_ACP_CWD": "~",
```

### Key Environment Variables for Sampling

**Local ACP agent:**

* **`MCPSample_ACP_CMD`:** The command line that launches the local Agent Context Protocol (ACP) agent (e.g. IBM Bob in ACP mode). Experience leans to using an absolute path. On Windows, launch the agent's executable directly (as `node` is above) rather than through a `.cmd` shim, so the process TSAR-MCP starts is the agent and cancellation can stop it.
* **`MCPSample_ACP_CWD`:** The directory the ACP agent runs in. Treat this and `MCPSample_ACP_CMD` together as a **capability grant**, not a convenience setting — especially with `--trust`, which suppresses the agent's own confirmation prompts. `~` (your home directory, on Windows and Unix) is a convenient default, but it lets the agent see all of it. An aspect that only transforms text (like `spellCheck`) has no meaningful workspace and can be pointed at a throwaway directory instead; an aspect that reasons about your code should be given the project you actually mean it to see, and may set its own workspace (via `Sampler.SetWorkspace()`).
* **Agent credentials** (`BOB_API_KEY` above) are *not* TSAR-MCP settings. The `env` block is inherited by the spawned agent, so the agent's own configuration belongs here too. Bob keys: [bob.ibm.com/admin/apikeys](https://bob.ibm.com/admin/apikeys)

**External AI (cURL egress):**

* **`MCPSample_AI_URL`:** Provider endpoint. A `%s` in the URL is substituted with `MCPSample_AI_MODEL` — **Google-native endpoints only**; OpenAI and Anthropic URLs are sent verbatim.
* **`MCPSample_AI_MODEL`:** Model identifier, which also selects the request format: a name containing `gemini` or `anthropic` picks that provider's payload shape, while `openai` appearing in the *URL* forces the OpenAI shape.
* **`MCPSample_AI_KEY`:** API key for the provider. Gemini: [aistudio.google.com/api-keys](https://aistudio.google.com/api-keys) — OpenAI: [platform.openai.com/api-keys](https://platform.openai.com/api-keys)

Together these three arm the external cURL egress path, used as a last resort when neither MCP client sampling nor a local ACP agent is available.

**Routing:**

* **`MCPSample_EXTERN_PRIORITY`:** `1`/`T`/`Y` steps the connected AI client aside so an external engine answers first (ACP, then cURL) — useful when the client's model is weaker than the one you have configured locally. It is a priority, not a prohibition: with no external engine available, the client still answers.

*For the full routing order and cancellation semantics, see the **[MCPServer Advanced Patterns Guide](./MCPServer_AdvancedPatterns.md)** (`MCPSyncSamplerEx`).*

## Troubleshooting

* **`-debug`:** Appending this argument to the TSAR-MCP execution command will output diagnostic information (typically routed to the client's debug logs or a local trace file, depending on your aspect implementation).
* **`-trace`:** Enables deeper runtime tracing within the `CommonC` foundation.
* **Connection Hangs:** If a remote SSH/Plink connection hangs upon initialization, verify that your SSH keys are correctly loaded in `ssh-agent` or Pageant. Any underlying prompt for a passphrase or fingerprint confirmation will halt the MCP handshake.
* **Pseudo-terminal (`-t`):** Do not allocate a pty when wiring a server to an AI client. TSAR-MCP is a raw `stdio` transport, and a remote pty's line discipline can interpret client teardown bytes (e.g. `Ctrl-Z`) as the Unix SUSP signal — suspending rather than ending the process and hanging the connection on shutdown. Omit `-t` (the OpenSSH `ssh.exe` examples already do). The one exception is *manual* interactive testing, where `-t` is needed so Plink provides a terminal to line-buffer your typed input; reserve it for that case only.
