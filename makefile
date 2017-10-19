BINARY = linux_niffs_test

############
#
# Paths
#
############

sourcedir = src
builddir = build


MKDIR = mkdir -p

###############
#
# Files and libs
#
###############

CFILES = niffs_internal.c \
	niffs_api.c

CFILES_TEST = main.c \
	niffs_test_emul.c \
	niffs_cfg_tests.c \
	niffs_func_tests.c \
	niffs_sys_tests.c \
	niffs_run_tests.c \
	testsuites.c \
	testrunner.c
	
INCLUDE_DIRECTIVES = -I./${sourcedir} -I./${sourcedir}/default -I./${sourcedir}/test 
CFLAGS_ALL = $(INCLUDE_DIRECTIVES) -DNIFFS_TEST_MAKE
CFLAGS = $(CFLAGS_ALL)  -fprofile-arcs -ftest-coverage
COMPILEROPTIONS_EXTRA = -Wall \
-Wall -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
-Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
-Wshadow -Wcast-align -Wchar-subscripts -Winline -Wnested-externs\
-Wredundant-decls
		
# address sanitization
CFLAGS += -fsanitize=address
LFLAGS += -fsanitize=address -fno-omit-frame-pointer -O
LIBS += -lasan
		
############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/default ${sourcedir}/test

OBJFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.o)

DEPFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.d)

OBJFILES = $(CFILES:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES) $(OBJFILES_TEST)

DEPENDENCIES = $(DEPFILES) $(DEPFILES_TEST) 

# link object files, create binary
${builddir}/$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@$(CC) $(CFLAGS) $(LINKEROPTIONS) $(LFLAGS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDENCIES)	
endif

# compile c files test
$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@$(MKDIR) $(@D)
		@$(CC) $(CFLAGS_ALL) -g -c -o $@ $<

# make dependencies
$(DEPFILES_TEST) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		$(RM) $@; \
		$(MKDIR) $(@D); \
		$(CC) $(CFLAGS_ALL) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@$(MKDIR) $(@D)
		@$(CC) $(CFLAGS) $(COMPILEROPTIONS_EXTRA) -g -c -o $@ $<

# make dependencies test
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		$(RM) $@; \
		$(MKDIR) $(@D); \
		$(CC) $(CFLAGS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$

all: $(BINARY) test

FILTER ?=

test: ${builddir}/$(BINARY)
ifdef $(FILTER)
		${builddir}/$(BINARY)
else
		${builddir}//$(BINARY) -f $(FILTER)
endif
		@for cfile in $(CFILES); do \
			gcov -o ${builddir} ${src}/$$cfile | \
			sed -nr "1 s/File '(.*)'/\1/p;2 s/Lines executed:(.*)/: \1 lines/p" | \
			sed 'N;s/\n/ /'; \
		done
	
test-failed: ${builddir}/$(BINARY)
		${builddir}/$(BINARY) _tests_fail
	
clean:
	@echo ... clean
	@$(RM) -r ${builddir}
	@$(RM) *.gcov
