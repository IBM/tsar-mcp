# MCPServer Advanced Patterns: Auxiliary Frameworks

The following sections describe auxiliary C++ frameworks that build on top of the base async and sampling patterns found in the core `MCPServer`. They provide structured, reusable, object-oriented abstractions for complex scenarios but require linking additional object files.

---

## MCPServerAsync — Thread Framework 

`MCPAsyncCore.h` + `MCPServerAsync.cpp` provide a structured work-item framework on top of the base deferred response pattern. Instead of manually managing threads and posting results, you derive from `MCPAsyncWork` and implement `BeginHandler()` / `CompleteHandler()`.

### When to Use
- Multiple async tools with shared thread-pool management.
- Need built-in abort/cancellation support (`AbortWork()`).
- Want a structured lifecycle: create → dispatch → begin → complete → destroy.

### Setup
```cpp
bool MCPServer_Asynchronous = true;
// Link: $(ASYNC_OBJS) — adds MCPServerAsync.obj
```

### Key Concepts
- Derive from `MCPAsyncWork` (full control) or `MCPAsyncWork_MCPOutput` (simplified — just set `MCPOutput` in `BeginHandler()`).
- Constructor requires `(JSON_Value &inId, JSON_Object &inArgs)` — the framework copies the id and arguments into local storage.
- `BeginHandler()` runs on a **worker thread**. Store results in members.
- `CompleteHandler()` runs on the **main thread**. Build and return MCPOutput.
- `Begin_AsyncWork(&WorkItem)` takes a pointer-to-pointer. On failure, the pointer remains valid and the caller must `delete` it.
- **Cancellation:** Integrates with MCP `notifications/cancelled`. The `Cancelled` flag suppresses the response automatically. Override `AbortWork()` to signal your `BeginHandler` to stop early.

### Work-Item Identity & Resume Ids
`get_Id()` is the work item's identity: the framework matches
`notifications/cancelled` against it (`idcmp()`, on the main thread) **and**
every response is addressed with it. It is mutex-guarded because it can change
while the work runs.

*Reference: See `MCPAsyncCore.h` for fully worked examples (`MCPAsyncWork_helloWorld` and `MCPAsyncWork_helloWorld2`) showing sync/async branching and error handling.*

---
