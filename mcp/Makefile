# ============================================================================
# MCPServer Master Makefile (GNU Make)
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
MCPBASE_DIR    = base
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

# --- Linker Flags ---
# Explicitly linking standard Linux libraries that might be required
LIBS = -lpthread -ldl

LDFLAGS =

# ============================================================================
# Object File Lists
# ============================================================================

CORE_OBJS = \
    $(O)/MCPServer.o \
    $(O)/MCPServerJSON.o

COMMONC_OBJS = \
    $(O)/ASThread.o \
    $(O)/AutoString.o \
    $(O)/ebcdic.o \
    $(O)/LevelTrace.o \
    $(O)/LinkList.o \
    $(O)/MEMprintf.o

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
    $(OUTDIR)/MCPServer_helloWorld

.PHONY: all clean cleanall _makedirs helloWorld

all: _makedirs $(ASPECTS)

helloWorld:  _makedirs $(OUTDIR)/MCPServer_helloWorld

_makedirs:
	@mkdir -p $(O)

clean:
	@rm -rf $(OUTDIR)

cleanall:
	@rm -rf build

# ============================================================================
# Link Rules
# ============================================================================

$(OUTDIR)/MCPServer_helloWorld: $(SHARED_OBJS)
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

# MCP base (MCPServer.cpp, MCPServerJSON.cpp)
$(O)/%.o: $(MCPBASE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC_CORE) -c $< -o $@

