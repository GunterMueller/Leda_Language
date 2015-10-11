### -*- mode: makefile -*-
###-----------------------------------------------------------------------------
### Title: GNU gcc make configuration file
###  Maintainer: Henry G. Weller
###-----------------------------------------------------------------------------

###-----------------------------------------------------------------------------
### Tools
###-----------------------------------------------------------------------------

CC = gcc-4.8 -std=c99 -D_GNU_SOURCE -m32
CXX = g++ -std = c++11
CPP = gcc -E

#CC = clang -std = c99 -D_GNU_SOURCE -m32
#CXX = clang++ -std = c++11
#CPP = clang -E

LD = $(CC)
AR = ar
RANLIB = ranlib
CC_DEPEND = $(CC) $(CFLAGS) $(CFLAGS_$*) -M -MG > _tmp.depend
CXX_DEPEND = $(CXX) $(CPPFLAGS) $(CPPFLAGS_$*) -M -MG > _tmp.depend
AS_DEPEND = $(CC) $(CPPFLAGS) $(CPPFLAGS_$*) -M -MG > _tmp.depend

###-----------------------------------------------------------------------------
### Compilation flags
###-----------------------------------------------------------------------------
CFLAGS_debug = -g -Wall -fno-inline -I. -I$(OBJDIR)
CFLAGS_opt = -O2 -Wall -I. -I$(OBJDIR)

CXXFLAGS_debug = -g -Wall -fno-inline -Wno-self-assign -Wno-parentheses-equality
CXXFLAGS_opt = -O2 -Wall

###-----------------------------------------------------------------------------
