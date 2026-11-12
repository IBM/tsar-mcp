# ============================================================================
# MCPServer Master Makefile (GNU Make)
#
# TSAR (Tools Slightly Above the Runtime)
# Copyright (c) 2026 International Business Machines Corporation
# Copyright (c) 2026 Eric Kass
# SPDX-License-Identifier: MIT
# ============================================================================
#
# Builds each MCPServer aspect as a separate executable.
#
# Usage:
#   make                       Build all aspects (Release)
#   make CFG=Debug             Build all aspects (Debug)
#   make helloWorld            Build MCPServer_helloWorld only
#   make clean                 Remove build artifacts for current CFG
#   make cleanall              Remove entire build directory
# ============================================================================

# --- Configuration (Release or Debug) ---
CFG ?= Release

ifeq ($(filter $(CFG),Release Debug),)
$(error CFG must be Release or Debug)
endif

# --- Source / Include Directories (relative from mcp/) ---
COMMONC_DIR    = ../base/CommonC
JSONPARSER_DIR = ../parsers/JSONParser
PARSER_DIR     = ../base/Parser
AICURL_DIR     = ../base/AIcURL
MCPBASE_DIR    = base
CLIENT_DIR     = client
MCPSRV_DIR     = servers

# --- Build Output ---
OUTDIR = build/$(CFG)
O      = $(OUTDIR)/obj

# --- Tools ---
CXX = g++

# --- Compiler Flags ---
# Replaced Windows macros with standard Linux equivalents (-pthread for MT)
CXXFLAGS_BASE = -Wall -Wextra -pthread \
                -Wno-unused-parameter -Wno-unused-function \
                -Wno-missing-field-initializers -Wno-unused-variable \
                -D"UNIX" -D"_CONSOLE" -D"_MBCS" -D"_USE_EXPRESSION_S"

ifeq ($(CFG),Release)
CXXFLAGS_CFG = -O2 -D"NDEBUG"
else
CXXFLAGS_CFG = -O0 -g -D"_DEBUG"
endif

CXXFLAGS = $(CXXFLAGS_BASE) $(CXXFLAGS_CFG)

# Include sets
INC_CORE = -I"$(COMMONC_DIR)" -I"$(JSONPARSER_DIR)" -I"$(PARSER_DIR)" -I"$(MCPBASE_DIR)"
# MCPSampleEx.h pulls in AIcURL.h and ACPSyncSampler.h (angle-bracket includes).
INC_SAMPLEEX = -I"$(AICURL_DIR)" -I"$(CLIENT_DIR)"

# --- Linker Flags ---
# Explicitly linking standard Linux libraries that might be required
LIBS = -lpthread -ldl

# -rdynamic exports all symbols to the dynamic symbol table so that
# SnapStack's backtrace_symbols() can resolve function names at runtime.
LDFLAGS = -rdynamic

# ============================================================================
# Object File Lists
# ============================================================================

CORE_OBJS = \
    $(O)/MCPServer.o \
    $(O)/MCPServerJSON.o

# Optional: MCPAsyncWork thread framework (see MCPAsyncCore.h).
# Add $(ASYNC_OBJS) to the link rule of an aspect that derives MCPAsyncWork.
ASYNC_OBJS = \
    $(O)/MCPServerAsync.o

# Optional: JSON-RPC 2.0 client (XCPClient -> RPCClient -> ACPClient).
CLIENT_OBJS = \
    $(O)/XCPClient.o \
    $(O)/RPCClient.o \
    $(O)/ACPClient.o \
    $(O)/ACPSyncSampler.o

# Optional: MCP sampling (see MCPSampleCore.h) and the 2026-07-28 MRTR engine
# (MCPServerMRTR self-registers its hook: linking the object is sufficient).
SAMPLING_OBJS = \
    $(O)/MCPServerSample.o

MRTR_OBJS = \
    $(O)/MCPServerMRTR.o

# Optional: MCPSyncSamplerEx, the blocking hybrid sampler (MCP / ACP / cURL).
# Add $(SAMPLING_OBJS) $(MRTR_OBJS) $(ASYNC_OBJS) with it.
SAMPLEEX_OBJS = \
    $(O)/MCPSampleEx.o \
    $(O)/AIcURL.o \
    $(CLIENT_OBJS)

COMMONC_OBJS = \
    $(O)/ASThread.o \
    $(O)/AutoString.o \
    $(O)/cURLRunner.o \
    $(O)/ebcdic.o \
    $(O)/LevelTrace.o \
    $(O)/LinkList.o \
    $(O)/MEMprintf.o \
    $(O)/popenNT.o \
    $(O)/SnapStack.o

COMMONC_TIMER_OBJS = \
    $(O)/TimerThread.o

COMMONC_NET_OBJS = \
    $(O)/IPv6Parse.o \
    $(O)/SocketCB.o

JSONPARSER_OBJS = \
    $(O)/_JSONParser.o \
    $(O)/JSONObject.o \
    $(O)/JSONParser.o \
    $(O)/JSONParserDetect.o \
    $(O)/JSONParserExp.o \
    $(O)/JSONParserLex.o

PARSER_OBJS = \
    $(O)/Expression.o \
    $(O)/ExpressionSN.o \
    $(O)/FindNode.o \
    $(O)/LexicalParser.o \
    $(O)/PrintExpression.o

SHARED_OBJS = $(CORE_OBJS) $(COMMONC_OBJS) $(JSONPARSER_OBJS) $(PARSER_OBJS)

# ============================================================================
# Targets
# ============================================================================

ASPECTS = \
    $(OUTDIR)/MCPServer_helloWorld \
    $(OUTDIR)/MCPServer_netstat \
    $(OUTDIR)/MCPServer_portScan \
    $(OUTDIR)/MCPServer_setReminder \
    $(OUTDIR)/MCPServer_spellCheck \
    $(OUTDIR)/MCPServer_wordArt

.PHONY: all clean cleanall _makedirs helloWorld netstat portScan setReminder spellCheck wordArt

all: _makedirs $(ASPECTS)

helloWorld:   _makedirs $(OUTDIR)/MCPServer_helloWorld
netstat:      _makedirs $(OUTDIR)/MCPServer_netstat
portScan:     _makedirs $(OUTDIR)/MCPServer_portScan
setReminder:  _makedirs $(OUTDIR)/MCPServer_setReminder
spellCheck:   _makedirs $(OUTDIR)/MCPServer_spellCheck
wordArt:      _makedirs $(OUTDIR)/MCPServer_wordArt

_makedirs:
	@mkdir -p $(O)

clean:
	@rm -rf $(OUTDIR)

cleanall:
	@rm -rf build

# ============================================================================
# Link Rules
# ============================================================================

$(OUTDIR)/MCPServer_helloWorld: $(SHARED_OBJS) $(O)/MCPServer_helloWorld.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OUTDIR)/MCPServer_netstat: $(SHARED_OBJS) $(ASYNC_OBJS) $(O)/MCPServer_netstat.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OUTDIR)/MCPServer_portScan: $(SHARED_OBJS) $(COMMONC_NET_OBJS) $(O)/MCPServer_portScan.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OUTDIR)/MCPServer_setReminder: $(SHARED_OBJS) $(COMMONC_TIMER_OBJS) $(O)/MCPServer_setReminder.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OUTDIR)/MCPServer_spellCheck: $(SHARED_OBJS) $(ASYNC_OBJS) $(SAMPLING_OBJS) $(SAMPLEEX_OBJS) $(MRTR_OBJS) $(O)/MCPServer_spellCheck.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OUTDIR)/MCPServer_wordArt: $(SHARED_OBJS) $(O)/MCPServer_wordArt.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

# ============================================================================
# Compile Rules - GNU Make Pattern Rules
# ============================================================================

# CommonC
$(O)/%.o: $(COMMONC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I"$(COMMONC_DIR)" -c $< -o $@

# JSONParser
$(O)/%.o: $(JSONPARSER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

# Parser
$(O)/%.o: $(PARSER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I"$(COMMONC_DIR)" -I"$(PARSER_DIR)" -c $< -o $@

# MCP base (MCPServer.cpp, MCPServerJSON.cpp, MCPServerAsync.cpp,
#           MCPServerSample.cpp, MCPServerMRTR.cpp, MCPSampleEx.cpp)
$(O)/%.o: $(MCPBASE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) $(INC_SAMPLEEX) -c $< -o $@

# AIcURL
$(O)/%.o: $(AICURL_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) $(INC_SAMPLEEX) -c $< -o $@

# JSON-RPC 2.0 client (XCPClient, RPCClient, ACPClient, ACPSyncSampler, MCPClient)
$(O)/%.o: $(CLIENT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) $(INC_SAMPLEEX) -c $< -o $@

# MCP servers - helloWorld
$(O)/%.o: $(MCPSRV_DIR)/helloWorld/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

# MCP servers - netstat
$(O)/%.o: $(MCPSRV_DIR)/netstat/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

# MCP servers - portScan
$(O)/%.o: $(MCPSRV_DIR)/portScan/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

# MCP servers - setReminder
$(O)/%.o: $(MCPSRV_DIR)/setReminder/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

# MCP servers - spellCheck
$(O)/%.o: $(MCPSRV_DIR)/spellCheck/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) $(INC_SAMPLEEX) -c $< -o $@

# MCP servers - wordArt
$(O)/%.o: $(MCPSRV_DIR)/wordArt/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

