###-----------------------------------------------------------------------------
### Title: Makefile
###  Maintainer: Henry G. Weller
###   make
###    Build 'lc' for the default target (opt)
###   make TARGET=debug
###    Build debug
###-----------------------------------------------------------------------------
P_DIR = .
include $(P_DIR)/Make/Makefile.config

###-----------------------------------------------------------------------------
### Default build
###-----------------------------------------------------------------------------
.PHONY: all
all: lc doc

.PHONY: lc
lc:
	$M $(MAKE) -C Src

###-----------------------------------------------------------------------------
### Test commands
###-----------------------------------------------------------------------------
.PHONY: test
test:
	$M $(MAKE) -C Test

###-----------------------------------------------------------------------------
### Miscellaneous commands
###-----------------------------------------------------------------------------
include $(P_DIR)/Make/Makefile.rules

README.org: index.org
	$M sed 's%file:%http://henry.github.com/Leda/%' $< > $@

.PHONY: README
README: index.html README.org

.PHONY: doc
doc: README
	$M $(MAKE) -C Doc

.PHONY: pack
pack:
	$M tar czf leda.tar.gz \
	    Makefile \
	    Make \
	    Src/Makefile Src/gram.y Src/lexer.l Src/*.c Src/*.h \
	    Doc/ReleaseNotes.org \
	    Doc/cprog.sty Doc/refmanual.tex Doc/grammar.tex Doc/std.tex \
	    Doc/trouble.tex \
	    Test/README.org Test/Makefile Test/*.led Test/chap17input \
	    Test/concordanceInput

.PHONY: clean
clean: distclean
	$M $(MAKE) -C Src distclean
	$H $(MAKE) -C Doc distclean
	$H $(MAKE) -C Test distclean

###-----------------------------------------------------------------------------
