# How to Create an MCPServer Aspect

**For LLMs and Developers** — This guide explains how to build a new MCP server tool by creating an "aspect" — a single `.cpp` file that plugs into the MCPServer framework.

## What is an Aspect?

The MCPServer is a C++ implementation of the [Model Context Protocol](https://modelcontextprotocol.io/specification/). Each **aspect** is a single `.cpp` file that defines the server's personality: its name, tools, prompts, and handlers. Each aspect compiles into its own executable (e.g., `MCPServer_helloWorld.exe`, `MCPServer_portScan.exe`).

The framework (`MCPServer.cpp`) handles all JSON-RPC protocol mechanics — parsing requests, dispatching methods, validating output, stdin/stdout I/O. Your aspect only implements the *what*, never the *how* of the protocol.

## Reference Template: `MCPServer_helloWorld.cpp`

Start by copying `MCPServer_helloWorld.cpp`. It is a minimal, working aspect.

## Required Sections (in order)

### 1. Includes and Identity

```cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LevelTrace.h>
#include <MEMprintf.h>

#include <MCPServerCore.h>

const char *MCPServer_Name = "MCPServer_myAspect";     // Executable identity.
const char *MCPServer_Version = "0.1.0";
const char *MCPServer_Capabilities = "\"tools\":{}";    // MCP capabilities.
bool MCPServer_Asynchronous = false;                    // true for async tools.
```

- `MCPServer_Name` — used in trace filenames and `serverInfo` sent to the client.
- `MCPServer_Capabilities` — typically `"\"tools\":{}"`. Add `"prompts"`, `"resources"`, etc. as needed.
- `MCPServer_Asynchronous` — set `true` if any tool needs deferred responses (worker threads).

### 2. Test Cases

```cpp
static const char* MCPInputTestRequest_MyCall =
        "{"
         " \"jsonrpc\": \"2.0\","
         " \"id\": 3,"
         " \"method\": \"tools/call\","
         " \"params\": {"
         "   \"name\": \"myTool\","
         "   \"arguments\": { \"key\": \"value\" }"
         " }"
         "}";

const char* MCPInputTestRequests[] =
        {
        MCPInputTestRequest_MyCall,
        NULL                            // NULL-terminated array.
        };
```

Run tests with: `MCPServer_myAspect.exe Test`
*Note: A successful test will output the raw JSON-RPC response directly to the console.*

### 3. Init / Shutdown

```cpp
bool MCPServer_OnInitialize(MCPInputRequest &MCPRequest)
        {
        // Called once when the MCP client sends "initialize".
        return true;
        }

bool MCPServer_OnShutdown()
        {
        // Called when stdin closes (client disconnect).
        return true;
        }
```

### 4. Tool Directory

Register tools via the `MCPToolInfo` array. The framework auto-generates the `tools/list` response from this.

```cpp
const unsigned MCPToolInfoCount = 1;

MCPToolInfo_t MCPToolInfo[] =
        {
                {
                // Name
                "myTool",
                // Description (one line, no newlines — token-efficient)
                "Does something useful. Returns result as text.",
                1,                      // nProperties
                // InputSchema {Properties}
                        {
                        "\"key\":{\"type\":\"string\","
                                 "\"description\":\"The input key\"}"
                        },
                // [Required]
                "\"key\""
                }
        };
```

### 5. Tool Handlers — The Two-Layer Pattern

Every tool follows this pattern:

```
MCPtool_xxx()           — Parse arguments, validate, call FormMCPResponse_xxx()
FormMCPResponse_xxx()   — Build result text, call FormMCPResponse_Text()
```

**Memory Management Note:** You do **not** need to manage the memory of the `char*` buffer returned by these handlers. Function handlers returning an `MCPOutput` JSON buffer must assume it will be automatically freed by the caller (`MCPServer.cpp`) after being sent.

**Response builder** — builds the text and wraps it:

```cpp
char* FormMCPResponse_myTool(JSON_Value &idRequest, const char *Key)
        {
        MemoryPrintf Summary;
        Summary.printf("Result for key: %s", Key);
        return FormMCPResponse_Text(&idRequest, Summary.GetBuffer());
        }
```

**Tool handler** — parses and validates the request:

```cpp
char* MCPtool_myTool(JSON_Value &idRequest, JSON_Object &Arguments)
        {
        const char *ProcName = "MCPtool_myTool";
        const char *Key = Arguments.Find_member_string("key");
        if (!Key)
                {
                TERROR(("%s: Missing Argument: 'key'",ProcName));
                return FormMCPResponse_ERROR(&idRequest,
                                             MCPErrorCode_InvalidParms,
                                             "Invalid Params",
                                             "Missing parameter: key");
                }
        return FormMCPResponse_myTool(idRequest, Key);
        }
```

### 6. Tool Dispatch

Route tool names to handlers:

```cpp
char* Handle_tools_call(JSON_Value &idRequest,
                        const char *ToolName,
                        JSON_Object &Arguments)
        {
        char *MCPOutput = NULL;
        if (strcmp(ToolName,"myTool") == 0)
                {
                MCPOutput = MCPtool_myTool(idRequest, Arguments);
                }
        return MCPOutput;       // NULL = "unknown tool" (framework handles).
        }
```

### 7. Dormant Handlers

These are required by the linker. Copy them verbatim from `MCPServer_helloWorld.cpp` and activate as needed:

```cpp
void Handle_notification(MCPInputRequest &MCPRequest) { return; }
char* Handle_sampling_response(MCPInputRequest &MCPRequest) { return NULL; }
char* Handle_notify_action(void *UserPtr) { return NULL; }
void Discard_notify_action(void *UserPtr) { return; }
char* Handle_completion_complete(JSON_Value &idRequest, const char *RefType, const char *RefName, const char *ArgName, const char *ArgValue) { return NULL; }
const unsigned MCPPromptInfoCount = 0;
MCPPromptInfo_t MCPPromptInfo[] = { {0} };
char* Handle_prompts_get(JSON_Value &idRequest, const char *PromptName, JSON_Object &Arguments) { return NULL; }
char* Handle_method(MCPInputRequest &MCPRequest) { return NULL; }
```

## Build System & VSCode Registration

To add a new aspect, add it to both Makefiles in the `mcp/` directory:

**GNU Make (`Makefile`) — Linux:**
```makefile
# Named target:
myAspect: _makedirs $(OUTDIR)/MCPServer_myAspect

# Link rule:
$(OUTDIR)/MCPServer_myAspect: $(SHARED_OBJS) $(O)/MCPServer_myAspect.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

# Compile rule:
$(O)/%.o: $(MCPSRV_DIR)/myAspect/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@
```
Build: `make myAspect` (or `make myAspect CFG=Debug`)

**NMAKE (`Makefile.nmake`) — Windows:**
```makefile
# Named target:
myAspect: _makedirs $(OUTDIR)\MCPServer_myAspect.exe

# Link rule:
$(OUTDIR)\MCPServer_myAspect.exe: $(SHARED_OBJS) $(O)\MCPServer_myAspect.obj
    $(LINK) $(LDFLAGS) /OUT:$@ $** $(LIBS)

# Compile rule:
{$(MCPSRV_DIR)\myAspect\}.cpp{$(O)\}.obj:
    $(CC) $(CFLAGS) $(INC_CORE) /Fo$@ $<
```
Build: `nmake myAspect` (or `nmake myAspect CFG=Debug`)

**VSCode `mcp.json`:**
```json
{
  "servers": {
    "myAspect": {
      "type": "stdio",
      "command": "path/to/MCPServer_myAspect.exe"
    }
  }
}
```

## Advanced Native Features

The core framework (`MCPServer.cpp`) inherently supports:
* **Async Tools:** Set `MCPServer_Asynchronous = true` and return `Return_MCPOutput_Pending()`. Use `PostMCPOutput()` or `PostNotifyAction()` from your worker thread.
* **Sampling:** Send `sampling/createMessage` requests to the LLM and catch the result in `Handle_sampling_response()`. Use `FormMCPRequest_sampling()` to build a sampling request.
* **Prompts:** Populate `MCPPromptInfo[]` and implement `Handle_prompts_get()`. Use `FormMCPResponse_prompt()` to respond to a prompt request. 

*For structured, object-oriented implementations of Async and Sampling, see `MCPServer_AdvancedPatterns.md`.*

## Checklist for a New Aspect
- [ ] Copy `MCPServer_helloWorld.cpp` → `MCPServer_myAspect.cpp`
- [ ] Set Identity (`MCPServer_Name`, `MCPServer_Version`)
- [ ] Define `MCPToolInfo[]`
- [ ] Implement `FormMCPResponse_xxx()` and `MCPtool_xxx()`
- [ ] Wire dispatch in `Handle_tools_call()`
- [ ] Add test cases and verify via `MCPServer_myAspect.exe Test`
- [ ] Add Makefile target & VSCode Registration