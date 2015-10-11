###-----------------------------------------------------------------------------
### Title: Makefile
###  Maintainer: Henry G. Weller
###   make all
###    Build 'refmanual.pdf and ReleaseNotes.html'
###-----------------------------------------------------------------------------
P_DIR = ..
DIR = /
include $(P_DIR)/Make/Makefile.config

###-----------------------------------------------------------------------------

.PHONY: all
all: refmanual.pdf ReleaseNotes.html

refmanual.pdf: refmanual.tex grammar.tex std.tex  trouble.tex cprog.sty
	$M pdflatex refmanual.tex
	$M pdflatex refmanual.tex

ReleaseNotes.html: ReleaseNotes.org

###-----------------------------------------------------------------------------
### Include the build rules
###-----------------------------------------------------------------------------
include $(P_DIR)/Make/Makefile.build

###-----------------------------------------------------------------------------
