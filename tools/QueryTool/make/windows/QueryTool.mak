#############################################################################
# QueryTool.mak - Build QueryTool                                           #
#                                                                           #
#       Targets:                                                            #
#                                                                           #
#               QueryTool.exe            - The QueryTool.          			#
#                                                                           #
#############################################################################

#############################################################################
#                               Configuration                               #
#############################################################################

.SUFFIXES : .obj .cpp

CC = cl.exe /c
CCShared = $(CC)

LD_FLAGS= /debug /nologo /subsystem:console /incremental:no /manifest
LD = link.exe $(LD_LIBS) $(LD_FLAGS)
MT = mt.exe -manifest $@.manifest -outputresource:$@;1

LDShared_FLAGS=/debug /nologo /incremental:no /manifest
LDShared = link.exe $(LDShared_FLAGS)
MTShared = mt.exe -manifest $@.manifest -outputresource:$@;2

COMMONCDIR=..\..\..\..\base\CommonC
PARSERDIR=..\..\..\..\base\Parser
PRCEDPPDIR=..\..
PRCEDDIR=..\..
QueryToolDIR=..\..

IBMINCDIR="C:\Program Files (x86)\IBM\Client Access\Toolkit\Include"
IBMLIBDIR="C:\Program Files (x86)\IBM\Client Access\Toolkit\Lib"

OUTDIR=.

INCLUDEDIR = \
        /I$(COMMONCDIR) \
        /I$(PARSERDIR) \
		/I$(PRCEDPPDIR) \
		/I$(PRCEDDIR) \
		/I$(IBMINCDIR) \
        /I$(QueryToolDIR)

CC_FLAGS_DBG = \
        /TP \
        /Gz \
        /Zi \
        /Od \
        /EHsc \
        /MTd \
        /GF \
        /W3 \
        /D "_WIN32_WINNT=0x0501" \
        /D "WIN32" \
        /D "_DEBUG" \
        /D "_WINDOWS" \
        /D "_MBCS"

CC_FLAGS_OPT = \
        /TP \
        /O2 \
        /Ob1 \
        /Oi \
        /Ot \
        /Oy \
        /EHsc \
        /MT \
        /GF \
        /D "_WIN32_WINNT=0x0501" \
        /D "WIN32" \
        /D "NDEBUG" \
        /D "_WINDOWS" \
        /D "_MBCS"

CC_FLAGS = $(CC_FLAGS_OPT)

#############################################################################
#                       Microsoft Runtime Libraries                         #
#############################################################################

MS_LIBS= \
		kernel32.lib \
		user32.lib \
		gdi32.lib \
		winspool.lib \
		comdlg32.lib \
		advapi32.lib \
		shell32.lib \
		uuid.lib
		
#############################################################################
#                               IBM Libraries								#
#############################################################################

IBMLIBPATH=/LIBPATH:$(IBMLIBDIR)

IBM_LIBS= cwbapi.lib

#############################################################################
#                               Object Files                                #
#############################################################################

COMMONCOBJ = \
        ASThread.obj \
        ebcdic.obj \
        HexDump.obj \
        LevelTrace.obj \
        LinkList.obj \
        LoadLib.obj \
		MainArguments.obj

PARSEROBJECTS = \
		LexicalParser.obj \
		SQLLex.obj
	
PRCEDPPOBJECTS = \
		PRCEDpp.obj \
		PRCEDPrint.obj \
		QxdaUTIL.obj

QueryToolOBJ = \
		SimpleSqlParser.obj \
		SelectBuilder.obj \
		SqlToken.obj \
		util.obj \
		QueryTool.obj
           
#############################################################################
#                               Targets                                     #
#############################################################################

COMPILE = $(CC) $(CC_FLAGS) /I. $(INCLUDEDIR)

all:    QueryTool.exe

clean:
        -@del /f /q *.obj
        -@del /f /q *.res
        -@del /f /q *.dll
        -@del /f /q *.exe
        -@del /f /q *.exp
        -@del /f /q *.lib
        -@del /f /q *.pdb
        -@del /f /q *.manifest

QueryTool.exe: $(COMMONCOBJ) \
			  $(PRCEDPPOBJECTS) \
			  $(PARSEROBJECTS) \
              $(QueryToolOBJ)
        $(LD) $(COMMONCOBJ) \
			  $(PRCEDPPOBJECTS) \
              $(PARSEROBJECTS) \
			  $(QueryToolOBJ) \
			  $(IBMLIBPATH) \
			  $(IBM_LIBS) \
              $(MS_LIBS) \
              /pdb:"$(OUTDIR)\QueryTool.pdb" \
              /out:"$(OUTDIR)\QueryTool.exe"
        $(MT)

#############################################################################
#                               Rules                                       #
#############################################################################

{$(COMMONCDIR)}.cpp.obj :
        $(COMPILE) -Fo$@ $<

{$(PARSERDIR)}.cpp.obj :
        $(COMPILE) -Fo$@ $<
		
{$(PRCEDPPDIR)}.cpp.obj :
        $(COMPILE) -Fo$@ $<

{$(QueryToolDIR)}.cpp.obj :
        $(COMPILE) -Fo$@ $<

QueryTool.res : $(QueryToolDIR)\QueryTool.rc
        rc $(INCLUDEDIR) $(QueryToolDIR)\QueryTool.rc
        @move $(QueryToolDIR)\QueryTool.res .
