##===- projects/bytesflops/lib/byfl/Makefile -----------------*- Makefile -*-===##

######################################
# Build the libbyfl run-time library #
# (part of the Bytesflops tool)	     #
#				     #
# By Scott Pakin <pakin@lanl.gov>    #
######################################

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

include $(LEVEL)/Makefile.db

#
# Build a bitcode library.
#
LIBRARYNAME = byfl
BYTECODE_LIBRARY = 1

ifeq ($(ENABLE_BYFL_DB), 1)
BF_DB_CPP = byfl-db.cpp
BF_DB_CPP_FLAGS = -I$(MYSQLCPPCONN_INCLUDEDIR) -fexceptions
else
BF_DB_CPP = byfl-db-stubs.cpp
BF_DB_CPP_FLAGS = 
endif

SOURCES = byfl.cpp $(BF_DB_CPP) byfl-binary.cpp reuse-dist.cpp symtable.cpp threading.cpp ubytes.cpp vectors.cpp tallybytes.cpp

BUILT_SOURCES = opcode2name.cpp opcode2name.h
EXTRA_DIST = cachemap.h opcode2name
CPPFLAGS += -I$(PROJ_SRC_ROOT)/lib/include $(BF_DB_CPP_FLAGS)

#
# Specify how to create opcode2name.cpp and opcode2name.h
#
opcode2name.cpp opcode2name.h: opcode2name
	$(PROJ_SRC_DIR)/opcode2name '$(CPP) -I$(LLVM_SRC_ROOT)/include' opcode2name.cpp opcode2name.h

clean-local::
	$(RM) -f $(BUILT_SOURCES)

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

# ======================================================================

#
# Include bitcode support that was obnoxiously removed at some point
# from Makefile.llvm.rules.  (We want the Byfl run-time library to be
# eligible for link-time optimization.)
#

.SUFFIXES: .bc .ll

ifdef BYTECODE_DESTINATION
BytecodeDestDir := $(BYTECODE_DESTINATION)
else
BytecodeDestDir := $(DESTDIR)$(PROJ_libdir)
endif

ObjectsBC := $(BaseNameSources:%=$(ObjDir)/%.bc)
DestBytecodeBlob = $(BytecodeDestDir)/lib$(LIBRARYNAME).bc

install-local:: $(DestBytecodeBlob)

$(DestBytecodeBlob): $(ObjectsBC)
	$(Echo) Installing $(BuildMode) Bytecode File $(DestBytecodeBlob)
	$(Verb) llvm-link -o $(DestBytecodeBlob) $(ObjectsBC)

BCCompile.CXX = $(LLVMCXX) $(CPP.Flags) $(CXX.Flags) $(CXXFLAGS) $(CPPFLAGS) \
                $(TargetCommonOpts) $(CompileCommonOpts)

BC_DEPEND_OPTIONS = -MMD -MP -MF "$(ObjDir)/$*.bc.d.tmp" \
	-MT "$(ObjDir)/$*.ll" -MT "$(ObjDir)/$*.bc.d"

# If the build succeeded, move the dependency file over, otherwise
# remove it.
BC_DEPEND_MOVEFILE = then $(MV) -f "$(ObjDir)/$*.bc.d.tmp" "$(ObjDir)/$*.bc.d"; \
                     else $(RM) "$(ObjDir)/$*.bc.d.tmp"; exit 1; fi

$(ObjDir)/%.ll: %.cpp $(ObjDir)/.dir $(BUILT_SOURCES) $(LLVMCXX)
	$(Echo) "Compiling $*.cpp for $(BuildMode) build (bytecode)"
	$(Verb) if $(BCCompile.CXX) $(BC_DEPEND_OPTIONS) \
			$< -o $(ObjDir)/$*.ll -S $(LLVMCC_EMITIR_FLAG) ; \
	        $(BC_DEPEND_MOVEFILE)

# make the C and C++ compilers strip debug info out of bytecode libraries.
ifdef DEBUG_RUNTIME
$(ObjectsBC): $(ObjDir)/%.bc: $(ObjDir)/%.ll $(LOPT)
	$(Echo) "Compiling $*.ll to $*.bc for $(BuildMode) build (bytecode)"
	$(Verb) $(LOPT) $< -std-compile-opts -o $@
else
$(ObjectsBC): $(ObjDir)/%.bc: $(ObjDir)/%.ll $(LOPT)
	$(Echo) "Compiling $*.ll to $*.bc for $(BuildMode) build (bytecode)"
	$(Verb) $(LOPT) $< -std-compile-opts -strip-debug -o $@
endif


#---------------------------------------------------------
# Provide rule to build .bc files from .ll sources,
# regardless of dependencies
#---------------------------------------------------------
$(ObjDir)/%.bc: %.ll $(ObjDir)/.dir $(LLVMAS)
	$(Echo) "Compiling $*.ll for $(BuildMode) build"
	$(Verb) $(LLVMAS) $< -f -o $@


# Include bitcode dependency files if using bitcode libraries
ifdef BYTECODE_LIBRARY
DependFiles += $(DependSourceFiles:%=$(PROJ_OBJ_DIR)/$(BuildMode)/%.bc.d)
endif
