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

CFILES_TEST = niffs.c

CFILES = main.c \
	niffs_test_emul.c \
	niffs_func_tests.c \
	niffs_cfg_tests.c \
	testsuites.c \
	testrunner.c
INCLUDE_DIRECTIVES = -I./${sourcedir} -I./${sourcedir}/default -I./${sourcedir}/test 
CFLAGS = $(INCLUDE_DIRECTIVES) 
CFLAGS_TEST = $(CFLAGS) -Wall -fprofile-arcs -ftest-coverage
		
############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/default ${sourcedir}/test

OBJFILES = $(CFILES:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d)

OBJFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.o)

DEPFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES) $(OBJFILES_TEST)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(CFLAGS_TEST) $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)

-include $(DEPENDENCIES)	   	

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS) -g -c -o $@ $<

# make dependencies
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(CFLAGS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

# compile c files test
$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS_TEST) -g -c -o $@ $<

# make dependencies test
$(DEPFILES_TEST) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(CFLAGS_TEST) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY) test

mkdirs:
	-@${MKDIR} ${builddir}

test: $(BINARY)
		./build/$(BINARY)
		@for cfile in $(CFILES_TEST); do \
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
