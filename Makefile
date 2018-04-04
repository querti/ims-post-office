##
 # Subor          Makefile
 # Verzia         1.2
 # Datum 		  November 2016
 #
 # Predmet        IMS (2016)
 # Projekt        Model posty
 #
 # Authory        Lubomir Gallovic, 3BIT, [team-leader]
 #                Michal Ormos, 3BIT
 # 
 # Faculta        Fakulta Informacnych Technologii,
 #                Vysoke uceni technicke v Brne,
 #                Czech Republic
 #
 # E-mails        xgallo03@stud.fit.vutbr.cz
 #                xormos00@stud.fit.vutbr.cz
 #
 # Popis	      Makefile pre subor model.cc
 #

PROJ=model
PROGS=$(PROJ)
CC=g++
CFLAGS=-std=c++11 -Wall -pedantic
LIBS=-lsimlib

all: $(PROGS)

$(PROJ): $(PROJ).cc
	$(CC) $(CFLAGS) $(PROJ).cc -o $(PROJ) $(LIBS)

run: ${PROJ}
	./${PROJ}
