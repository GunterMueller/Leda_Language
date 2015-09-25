### Copyright 1993-2015 Timothy A. Budd
###-----------------------------------------------------------------------------
##  This file is part of
### ---     Leda: Multiparadigm Programming Language
###-----------------------------------------------------------------------------
##
##  Leda is free software: you can redistribute it and/or modify it under the
##  terms of the MIT license, see file "COPYING" included in this distribution.
##
###-----------------------------------------------------------------------------
### Title: Makefile
###  Maintainer: Henry G. Weller
###   make
###    Build 'lc' for the default target
###-----------------------------------------------------------------------------

all: lc doc

.PHONY: lc
lc:
	$(MAKE) -C Src

.PHONY: doc
doc:
	$(MAKE) -C Doc

.PHONY: test
test:
	$(MAKE) -C Test

.PHONY: pack
pack:
	@tar czf leda.tar.gz \
		Makefile \
		Src/ReadMe Src/Makefile Src/gram.y Src/lexer.l Src/*.c Src/*.h \
		Doc/cprog.sty Doc/refmanual.tex Doc/grammar.tex Doc/std.tex \
		Doc/trouble.tex \
		Test/ReadMe Test/Makefile Test/*.led Test/chap17input \
		Test/concordanceInput

.PHONY: clean
clean:
	$(MAKE) clean -C Src

###-----------------------------------------------------------------------------
