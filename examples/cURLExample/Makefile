# ============================================================================
# cURLExample Makefile (GNU Make)
#
# TSAR (Tools Slightly Above the Runtime)
# Copyright (c) 2026 International Business Machines Corporation
# Copyright (c) 2026 Eric Kass
# SPDX-License-Identifier: MIT
# ============================================================================

CFG ?= Release

ifeq ($(filter $(CFG),Release Debug),)
$(error CFG must be Release or Debug)
endif

COMMONC_DIR = ../../base/CommonC
JSONPARSER_DIR = ../../parsers/JSONParser
PARSER_DIR     = ../../base/Parser
AICURL_DIR     = ../../base/AIcURL
OUTDIR = build/$(CFG)
O      = $(OUTDIR)/obj

CXX = g++

CXXFLAGS_BASE = -Wall -Wextra -pthread \
                -Wno-unused-parameter -Wno-unused-function \
                -Wno-missing-field-initializers -Wno-unused-variable \
                -D"UNIX" -D"_CONSOLE" -D"_MBCS" -D"_USE_EXPRESSION_S"

ifeq ($(CFG),Release)
CXXFLAGS_CFG = -O2 -D"NDEBUG"
else
CXXFLAGS_CFG = -O0 -g -D"_DEBUG"
endif

INC_CORE = -I"$(COMMONC_DIR)" -I"$(JSONPARSER_DIR)" -I"$(PARSER_DIR)" -I"$(AICURL_DIR)"
CXXFLAGS = $(CXXFLAGS_BASE) $(CXXFLAGS_CFG) $(INC_CORE)

LIBS = -lpthread -ldl
LDFLAGS = -rdynamic

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

AICURL_OBJS = \
    $(O)/AIcURLJSON.o

OBJS = $(COMMONC_OBJS) $(JSONPARSER_OBJS) $(PARSER_OBJS) $(AICURL_OBJS) $(O)/cURLExample.o

.PHONY: all clean cleanall _makedirs

all: _makedirs $(OUTDIR)/cURLExample

_makedirs:
	@mkdir -p $(O)

clean:
	@rm -rf $(OUTDIR)

cleanall:
	@rm -rf build

$(OUTDIR)/cURLExample: $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(O)/%.o: $(COMMONC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(O)/%.o: $(JSONPARSER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(O)/%.o: $(PARSER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(O)/%.o: $(AICURL_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(O)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
