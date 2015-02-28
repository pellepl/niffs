ifndef niffs
$(warn defaulting path to generic niffs module, niffs variable not set)
niffs = ../generic/niffs
endif
FLAGS	+= -DCONFIG_BUILD_NIFFS
INC	+= -I${niffs}/src
CPATH	+= ${niffs}/src
CFILES	+= niffs_api.c
CFILES	+= niffs_internal.c
