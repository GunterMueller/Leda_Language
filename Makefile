#
#	makefile for Leda distribution
#

distr:
	tar cvf ledainterp.tar Makefile \
Src/ReadMe Src/Makefile Src/gram.y Src/lexer.l Src/*.c Src/*.h \
Doc/cprog.sty Doc/refmanual.tex Doc/grammar.tex Doc/std.tex Doc/trouble.tex \
Tests/ReadMe Tests/Makefile Tests/*.led Tests/chap17input \
Tests/concordanceInput

