###-----------------------------------------------------------------------------
### Title: Makefile
###  Maintainer: Henry G. Weller
###   make
###    Build 'lc' for the default target (debug)
###   make TARGET=opt
###    Build optimised
###-----------------------------------------------------------------------------
P_DIR = .
include $(P_DIR)/Make/Makefile.config

###-----------------------------------------------------------------------------
### Set the default target
###-----------------------------------------------------------------------------
TARGET = debug

###-----------------------------------------------------------------------------
### Default build
###-----------------------------------------------------------------------------
.PHONY: all
all: lc doc

.PHONY: lc
lc:
	$V $(MAKE) -C Src

###-----------------------------------------------------------------------------
### Test commands
###-----------------------------------------------------------------------------
.PHONY: test
test:
	$V $(MAKE) -C Test

###-----------------------------------------------------------------------------
### Miscellaneous commands
###-----------------------------------------------------------------------------
include $(P_DIR)/Make/Makefile.rules

README.org: index.org
	@sed 's%file:%http://henry.github.com/Leda/%' $< > $@

.PHONY: README
README: index.html README.org

.PHONY: doc
doc: README
	@$(MAKE) -C Doc

.PHONY: pack
pack:
	@tar czf leda.tar.gz \
		Makefile \
                Make \
		Src/ReadMe Src/Makefile Src/gram.y Src/lexer.l Src/*.c Src/*.h \
		Doc/cprog.sty Doc/refmanual.tex Doc/grammar.tex Doc/std.tex \
		Doc/trouble.tex \
		Test/ReadMe Test/Makefile Test/*.led Test/chap17input \
		Test/concordanceInput

###-----------------------------------------------------------------------------
