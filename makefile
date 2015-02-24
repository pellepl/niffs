BINARY = linux_niffs_test

############
#
# Paths
#
############

sourcedir = src
builddir = build


#############
#
# Build tools
#
#############

CC = gcc $(CFLAGS)
LD = ld
GDB = gdb
OBJCOPY = objcopy
OBJDUMP = objdump
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
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(CFLAGS) $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)

-include $(DEPENDENCIES)	   	

# compile c files test
$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS_ALL) -g -c -o $@ $<

# make dependencies
$(DEPFILES_TEST) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(CFLAGS_ALL) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS) $(COMPILEROPTIONS_EXTRA) -g -c -o $@ $<

# make dependencies test
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(CFLAGS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY) test

mkdirs:
	-@${MKDIR} ${builddir}

test: $(BINARY)
		./build/$(BINARY)
		@for cfile in $(CFILES); do \
			gcov -o ${builddir} ${src}/$$cfile | \
			sed -nr "1 s/File '(.*)'/\1/p;2 s/Lines executed:(.*)/: \1 lines/p" | \
			sed 'N;s/\n/ /'; \
		done
	
test_failed: all
		./build/$(BINARY) _tests_fail
	
clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.gcov
	@rm -f ${builddir}/*.gcno
	@rm -f ${builddir}/*.gcda
	@rm -f *.gcov
	@rm -f ${builddir}/*.elf
