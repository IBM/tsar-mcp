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
      "command": "P:\\Programs\\tsar-mcp\\mcp\\build\\Release\\MCPServer_helloWorld.exe",
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
        "Programs/tsar-mcp/mcp/build/Release/MCPServer_helloWorld",
        "-debug",
        "-trace"
      ]
    }
  }
}

```

## 3. Remote Execution (PuTTY / Plink)

If you are on a Windows environment and prefer to use the PuTTY suite for SSH management, you can use `PLINK.EXE`.

When using Plink, it is highly recommended to include the `-batch` flag. This disables all interactive prompts (like host key warnings), ensuring the standard output stream remains pure JSON-RPC data.

**Example: `mcp.json**`

```json
{
  "mcpServers": {
    "helloWorld_plink": {
      "type": "stdio",
      "command": "C:\\Util\\PuTTY\\PLINK.EXE",
      "args": [
        "-t",
        "-batch",
        "eric@taurus",
        "Programs/tsar-mcp/mcp/build/Release/MCPServer_helloWorld"
      ]
    }
  }
}

```

## Troubleshooting

* **`-debug`:** Appending this argument to the TSAR-MCP execution command will output diagnostic information (typically routed to the client's debug logs or a local trace file, depending on your aspect implementation).
* **`-trace`:** Enables deeper runtime tracing within the `CommonC` foundation.
* **Connection Hangs:** If a remote SSH/Plink connection hangs upon initialization, verify that your SSH keys are correctly loaded in `ssh-agent` or Pageant. Any underlying prompt for a passphrase or fingerprint confirmation will halt the MCP handshake.

```

