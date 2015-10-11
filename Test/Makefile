###-----------------------------------------------------------------------------
### Title: Makefile
###  Maintainer: Henry G. Weller
###   make
###    Regression tests for 'lc' for the default target.
###    Missing, chap6c which seg-faults
###-----------------------------------------------------------------------------
P_DIR = ..
DIR = /
include $(P_DIR)/Make/Makefile.config

###-----------------------------------------------------------------------------
### Set the default target
###-----------------------------------------------------------------------------
TARGET = opt

###-----------------------------------------------------------------------------
LC = $(OBJDIR)/lc

regression: $(LC)
	@$(LC) chap2a.led > log
	@$(LC) chap2b.led >> log
	@$(LC) chap2c.led >> log
	@$(LC) chap2d.led >> log
	@$(LC) chap2e.led >> log
	@$(LC) chap2f.led >> log
	@$(LC) chap3.led >> log
	@$(LC) chap4a.led >> log
	@$(LC) chap4b.led >> log
	@$(LC) chap4c.led >> log
	@$(LC) chap5.led >> log
	@$(LC) chap6a.led >> log
	@$(LC) chap6b.led >> log
	@$(LC) chap6d.led >> log
	@$(LC) chap6e.led >> log
	@$(LC) chap6f.led >> log
	@$(LC) chap7a.led >> log
	@$(LC) chap7b.led >> log
	@$(LC) chap7c.led >> log
	@$(LC) chap7d.led >> log
	@$(LC) chap8a.led >> log
	@$(LC) chap8c.led < concordanceInput >> log
	@$(LC) chap9.led >> log
	@$(LC) chap11.led >> log
	@$(LC) chap14.led >> log
	@$(LC) chap15a.led >> log
	@$(LC) chap15b.led >> log
	@$(LC) chap15c.led >> log
	@$(LC) chap15d.led >> log
	@$(LC) chap16.led >> log
	@$(LC) chap17.led < chap17input >> log
	@$(LC) chap19a.led >> log
	@$(LC) chap19b.led >> log
	@$(LC) chap19c.led >> log
	@$(LC) chap20a.led >> log
	@$(LC) chap20b.led >> log
	@$(LC) chap20c.led >> log
	@$(LC) -m 500000 chap20d.led >> log
	@$(LC) chap21.led >> log
	@diff ref log

TOCLEAN = log

###-----------------------------------------------------------------------------
### Include the build rules
###-----------------------------------------------------------------------------
include $(P_DIR)/Make/Makefile.build

###-----------------------------------------------------------------------------
