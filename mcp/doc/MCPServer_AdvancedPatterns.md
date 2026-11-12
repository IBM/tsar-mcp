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
// MCPServerAsync.cpp self-registers its hooks; MCPServer.cpp is unchanged.
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

If `BeginHandler()` performs a Multi-Round-Trip request (via a blocking sampler
or `MCPSyncRoundTrip`), the client resumes under a **new** id. Call
`AdoptResumeId(Source)` right after each blocking call to re-point the work at
the source's current id:

    Response = Sampler.BlockingSample(...);
    AdoptResumeId(Sampler);   // == set_Id(Sampler.get_resumeId())

Do this whether or not the call succeeded — the source always holds its current
id. Afterward, both cancellation (`idcmp()`) and responses (`get_Id()`) stay
correct, and no handler reaches into the source for the id. `AdoptResumeId`
accepts anything exposing `get_resumeId()` (`MCPSyncSamplerEx`,
`MCPSyncRoundTrip`).

*Reference: See `MCPAsyncCore.h` for fully worked examples (`MCPAsyncWork_helloWorld` and `MCPAsyncWork_helloWorld2`) showing sync/async branching and error handling.*

---

## MCPServerSample — Sampling Framework

`MCPSampleCore.h` + `MCPServerSample.cpp` provide a structured sampling manager on top of the base `Handle_sampling_response()` mechanism. Instead of manually correlating sampling responses by id, you derive from `MCPSampleRequest` and implement `RequestSampling()` / `OnResponse()`.

### When to Use
- Multiple concurrent sampling requests in flight.
- Need structured request/response correlation without manual id tracking.
- Multiple tools that each need sampling with different prompts.

### Setup
```cpp
// Link: $(SAMPLING_OBJS) — adds MCPServerSample.obj
// MCPServerSample.cpp self-registers its hooks; MCPServer.cpp is unchanged.
```

### Key Concepts
- Call `isSamplingSupported()` at any time to check whether the client advertised sampling capability.
- Derive from `MCPSampleRequest`. Constructor requires `(JSON_Value &inId, JSON_Object &inArgs)`.
- `RequestSampling()` — pure virtual. Build and return the `sampling/createMessage` JSON-RPC request using the inherited `FormSamplingRequest()` helper.
- `OnResponse(MCPInputRequest &MCPRequest)` — pure virtual. Receives the client's LLM result. Return the final `MCPOutput` for the original tool call.
- **Dispatch pattern:** `MCPOutput = RequestSample(&Request); if (Request) delete Request;`

*Reference: See `MCPSampleCore.h` for a fully worked `HelloWorldSample` example showing the complete translation flow.*

---

## MCPSyncSampler — Blocking Sampling for Worker Threads

`MCPSyncSampler` (defined in `MCPSampleCore.h`) bridges `MCPServerAsync` with the sampling framework — allowing a **worker thread** to issue a sampling request and **block** until the response arrives.

### When to Use
- A worker thread needs an LLM completion mid-execution to classify, summarize, or decide before continuing.
- You want synchronous "call and wait" semantics from within async work.
- Compatible with any threading model (`MCPAsyncWork`, `std::thread`, POSIX, etc.).

### Prerequisites
Both auxiliary frameworks must be linked:
- `$(ASYNC_OBJS)` — MCPServerAsync
- `$(SAMPLING_OBJS)` — MCPServerSample

Both self-register their hooks; `MCPServer.cpp` is unchanged.

### Key Concepts
- Constructor: `MCPSyncSampler(JSON_Value &inId)` — takes the tool's id.
- `BlockingSample(SystemPrompt, UserMessage, MaxTokens, TimeoutMS=0)` — builds the sampling request internally, sends it to the client via `PostMCPOutput()`, and blocks until the response arrives. `TimeoutMS` is optional; `0` (the default) waits indefinitely, relying on `CancelWait()` to unblock. Returns `MCPInputRequest*` (caller owns and deletes it), or `NULL` on failure/cancel.
- `CancelWait()` — unblocks a pending `BlockingSample()`. Use from `AbortWork()` to integrate with async cancellation.
- **Thread-safety:** A single mutex coordinates the blocking wait and response delivery. No deadlock risk; the main thread only signals and never waits.

### Usage
```cpp
MCPSyncSampler Sampler(idRequest);

MCPInputRequest *Response = Sampler.BlockingSample(
        "You are a classifier.",
        UserText,
        100);
if (!Response)
        return;   // cancelled or client doesn't support sampling

// Inspect Response->Get_result() ...
delete Response;
```

*Reference: See `MCPSampleCore.h` for the `MCPAsyncWork_withSampling` example demonstrating `CancelWait()` integration and blocking wait semantics.*

---

## MCPServerMRTR — Multi Round-Trip Request Framework

`MCPMultiTripCore.h` + `MCPServerMRTR.cpp` provide a structured framework for **input-required round trips** (MCP 2026-07-28). Where the sampling frameworks correlate a `sampling/createMessage` response, MRTR generalizes the pattern: a tool emits an `input_required` result carrying a `requestState` token, and the client resumes the call — with a **new id** — echoing that token. The framework matches the resume back to the originating request. (Sampling is just one kind of input request; `FormMCPRequest_sampling()` is a specialization of `FormMCPResult_inputRequired()`.)

### When to Use
- A tool needs more input from the client mid-call (sampling, elicitation, or any client-side `method`) under the stateless 2026-07-28 protocol.
- You want structured `requestState` correlation without hand-rolling the token lifecycle, TTL reaping, or resume dispatch.

### Setup
```cpp
// Link: $(MRTR_OBJS) — adds MCPServerMRTR.obj
// MCPServerMRTR.cpp self-registers its hooks; MCPServer.cpp is unchanged.
```

### Callback Variant — `MCPRoundTripRequest`
- Derive from `MCPRoundTripRequest`. Constructor requires `(JSON_Value &inId, JSON_Object &inArgs)` — the framework copies the id and arguments into local storage.
- `RequestInput()` — pure virtual. Build and return the `input_required` result via `FormMCPResult_inputRequired()` (or the `requestState` overload of `FormMCPRequest_sampling()`), embedding the inherited `requestState`.
- `OnResume(MCPInputRequest &MCPRequest)` — pure virtual. Runs on the **main thread** when the client resumes. Return the final `MCPOutput` for the tool.
- **Dispatch:** `MCPOutput = RequestRoundTrip(&Request); if (Request) delete Request;`

### Blocking Variant — `MCPSyncRoundTrip`
- For **worker threads** (any threading model). Derive and implement `FormInputRequest(inId, requestState, va_list)` — unpack your varargs and return the `input_required` result.
- `BlockingRequest(TimeoutMS, ...)` — sends the `input_required` result and blocks until the resume arrives (or the timeout expires). `TimeoutMS` leads here (rather than trailing as in `BlockingSample`) because the variadic `...` factory args must come last. Returns `MCPInputRequest*` (caller owns and deletes it), or `NULL` on cancel/timeout.
- `CancelWait()` — unblocks a pending `BlockingRequest()`. Call it from `AbortWork()` to integrate with async cancellation.

### The Resume-Id Contract
The **original request id is consumed** by the `input_required` result — it has already been answered on the wire. Answer the final tool result with the **resume id**, never the original:
- Callback variant: use `MCPRequest.Get_id()` inside `OnResume()`.
- Blocking variant: use `get_resumeId()` (it returns the adopted resume id once the resume lands).
- Inside an `MCPAsyncWork`? Call `AdoptResumeId(Source)` after the round trip and answer `get_Id()` — this keeps **cancellation** matching the resume id as well. (See *MCPServerAsync → Work-Item Identity & Resume Ids*.)

Answering the original id instead double-answers it and leaves the resume hanging.

### Threading
Allocation and `RequestRoundTrip()` may run in any thread; `OnResume()` and TTL reaping run on the **main thread**.

*Reference: See `MCPMultiTripCore.h` for fully worked `HelloWorldSample` (callback) and `MCPAsyncWork_withSampling` (blocking) examples — the names mirror the sampling framework on purpose, showing that sampling still lives, just carried over the MRTR round trip under the 2026-07-28 protocol.*

---

## MCPSyncSamplerEx — Hybrid Blocking Sampler

`MCPSampleEx.h` + `MCPSampleEx.cpp` compose four samplers behind one `BlockingSample()` call and route to whichever is available: native MCP `sampling/createMessage` (`MCPSyncSampler`), the 2026-07-28 MRTR round trip (`MCPSyncMRTRSampler`), a local agent over ACP (`ACPSyncSampler`), and an external AI provider over cURL. It is a near drop-in replacement for `MCPSyncSampler` — the caller reads the reply with `ExtractSampleText()` and answers `get_resumeId()`.

### When to Use
- Your MCP client does not advertise `sampling` capability (e.g. IBM watsonx Orchestrate / "Bob").
- You want aspect code that works identically whether the LLM completion comes from the MCP client, a local agent, or an external API.
- A local coding agent speaks ACP, and you would rather sample through it than send prompts off the machine.
- You want to override built-in MCP sampling with a more capable engine (`MCPSample_EXTERN_PRIORITY`).
- Your client speaks the stateless 2026-07-28 protocol and expects sampling as an `input_required` round trip (enable with `SetMRTRSupported()`).

### Prerequisites
These auxiliary frameworks must be linked:
- `$(ASYNC_OBJS)` — MCPServerAsync
- `$(SAMPLING_OBJS)` — MCPServerSample
- `$(MRTR_OBJS)` — MCPServerMRTR
- `$(SAMPLEEX_OBJS)` — MCPSampleEx + AIcURL
- XCPClient — `ACPSyncSampler.cpp`, `ACPClient.cpp`, `RPCClient.cpp`, `XCPClient.cpp`, `popenNT.cpp` (the ACP transport)

All self-register their hooks; `MCPServer.cpp` is unchanged.

### Routing Logic
`MCPSyncSamplerEx::BlockingSample()` decides automatically. The order is
deliberate: **each step reaches further from the machine than the one before it.**

> **Notice:** the routing order is subject to change. Which engines are
> tried, and in what sequence, may be revised as engines are added. The
> one part intended to hold is that the external AI path answers **last** —
> do not build on the relative position of the others.

1. **MCP client sampling** — the client's own model. Nothing leaves the host that was not already going to the client. Routes through classic `sampling/createMessage`, or the **MRTR round trip** when `SetMRTRSupported(true)` was set for a 2026-07-28 client.
2. **Local ACP agent** — if `MCPSample_ACP_CMD` names one. A process on this machine, working in a directory you choose. This is the path for clients that cannot sample at all.
3. **External AI over cURL** — **last resort.** The only path that sends the prompt off the machine, to an endpoint and under a credential that you configure. It answers when nothing closer can.
4. **Nothing available** — returns `NULL`.

`MCPSample_EXTERN_PRIORITY` steps the client aside so an external engine (ACP, then cURL) goes first. It is a priority, not a prohibition: with no external engine configured, the client still answers.

### Environment Variables

**ACP path (local agent):**

| Variable | Purpose |
|---|---|
| `MCPSample_ACP_CMD` | Command line that starts the local ACP agent |
| `MCPSample_ACP_CWD` | Agent workspace, used when `SetWorkspace()` was not called |

**External AI path (egress):**

| Variable | Purpose |
|---|---|
| `MCPSample_AI_URL` | Provider endpoint URL (OpenAI, Google, Anthropic — auto-detected) |
| `MCPSample_AI_MODEL` | Model identifier (e.g. `gpt-4o`, `gemini-2.0-flash`) |
| `MCPSample_AI_KEY` | API key for the provider |

> **Setting the three `MCPSample_AI_*` variables arms an egress path.**
> Every prompt that reaches step 3 is sent to that endpoint under that
> credential. It is the only part of the server that transmits off the
> machine, and it is armed by configuration alone — which is why it sits
> last in the routing order rather than first.

**Routing:**

| Variable | Purpose |
|---|---|
| `MCPSample_EXTERN_PRIORITY` | `1`/`Y`/`T` = prefer an external engine (ACP, then cURL) over client sampling |

Use `isSamplingExSupported(MCPRequest)` to verify that the hybrid sampler will be able to produce a response — true if MCP client sampling, the MRTR path, the ACP path, or the external AI path is available. Only *likely*: it is a free function with no sampler to ask, so it cannot know whether the caller will later supply an ACP workspace.

*For where to set these variables in `mcp.json`, where provider API keys come from, and what the ACP working directory grants, see the **[MCPServer Wiring & Transport Guide](./MCPServer_WiringGuide.md)** (section 4).*

### Key Concepts
- Constructor: `MCPSyncSamplerEx(JSON_Value &inId)` — same as base class.
- `BlockingSample(SystemPrompt, UserMessage, MaxTokens, TimeoutMS=0)` — identical signature to `MCPSyncSampler` (`TimeoutMS` optional, `0` waits indefinitely). Routing is internal and invisible to the caller.
- `SetMRTRSupported(bool)` — enable the 2026-07-28 MRTR round-trip path; pass `isMRTRSamplingSupported(MCPRequest)`.
- `SetWorkspace(const char *Cwd)` — the directory the ACP agent runs in. Only the caller can know it, so there is no default worth guessing: unset falls back to `MCPSample_ACP_CWD`, and failing that the ACP path reports `no_workspace` and declines. A tool with no meaningful workspace — a pure text transform, say — can leave it unset and rely on the environment.
- `get_resumeId()` — answer the tool result with this id. It returns the original id on the classic, ACP, and external paths, and the adopted resume id once the MRTR path resumes, so answering it is always correct.
- `CancelWait()` — cancels **all four** paths: aborts an in-flight cURL request, unblocks a pending classic or MRTR wait, and for ACP both asks the agent to stop the turn (`session/cancel`) and abandons the wait for it. Returns `true` if any cancellation succeeded.
- `SetExternPriority(bool)` — override the per-instance routing preference at runtime (default comes from `MCPSample_EXTERN_PRIORITY` env var).

### Notes on the ACP Path
- ACP has **no system role**. The system prompt is prepended to the user message, separated by a blank line, and sent as one prompt.
- The agent process is **shared** across the server and started on first use. Each `BlockingSample()` opens and closes its own ACP *session* against it, so a second sample costs a session round trip, not a process spawn.
- Cancellation is two-part because ACP's is cooperative: `session/cancel` names a session rather than a turn, and the outstanding prompt answers with `stopReason: cancelled`. The sampler issues both the cancel and a unilateral wait-abort, so a wedged agent cannot hold the worker thread.
- `MCPSample_ACP_CMD` spawns a program and the workspace hands it a directory. Treat both as a capability boundary, not a convenience setting.

### Usage
```cpp
MCPSyncSamplerEx Sampler(idRequest);
Sampler.SetMRTRSupported(isMRTRSamplingSupported(MCPRequest));

MCPInputRequest *Response = Sampler.BlockingSample(
        "You are a spell checker.",
        UserText,
        200);
if (!Response)
        return;   // cancelled or no sampling available

const char *Corrected = ExtractSampleText(*Response);
// ... build the tool result, answering Sampler.get_resumeId() ...
delete Response;
```

*Reference: See `MCPSampleEx.h` for full documentation including routing details and cancellation semantics.*