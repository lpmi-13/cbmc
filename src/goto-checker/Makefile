SRC = bmc_util.cpp \
      incremental_goto_checker.cpp \
      goto_verifier.cpp \
      multi_path_symex_only_checker.cpp \
      properties.cpp \
      report_util.cpp \
      solver_factory.cpp \
      symex_coverage.cpp \
      symex_bmc.cpp \
      # Empty last line

INCLUDES= -I ..

include ../config.inc
include ../common

CLEANFILES = goto-checker$(LIBEXT)

all: goto-checker$(LIBEXT)

###############################################################################

goto-checker$(LIBEXT): $(OBJ)
	$(LINKLIB)
