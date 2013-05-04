#
# $Id: Makefile,v 1.3 1998/03/25 01:32:26 gray Exp $
#

HFLS	= skip.h
OBJS	= skip.o

DEFS	= $(DEFINES)
INCS	= -I. -I$(ROOT)/inc $(INCLUDES)
LIBS	= -L. -L$(ROOT)/lib $(LIBRARIES)

CFLAGS	= -g $(CCFLS) -Wall $(CCFLAGS)

PORTFLS	= ChangeLog NEWS README Makefile.all \
	skip.[ch] skip_loc.h skip_t.c
DOCSFLS	= README concurrent.overview cookbook.overview skipLists.c \
	skipLists.overview

LDFLAGS	= $(LLDFLAGS)
DESTDIR	= $(ROOT)
LIBRARY	= libskip.a
TEST	= skip_t

all :: $(LIBRARY)

clean ::
	$(CLEAN)
	rm -f $(LIBRARY)
	rm -f $(TEST)

install :: $(HFLS) $(LIBRARY)
	install -cev -m 444 $(HFLS) $(DESTDIR)/inc
	install -cevr -m 644 $(LIBRARY) $(DESTDIR)/lib

installftp :: README
	install -cev -o ftp -g ftp -m 444 README ftp/README.txt
	install -cev -o ftp -g ftp -m 444 $(HFLS) ftp/

port :: $(PORTFLS)
	@ answer 'Is it okay to remove the $(.TARGET) directory? '
	rm -rf $@
	mkdir $@ $@/docs
	cp $(PORTFLS) $@
	cd docs ; cp $(DOCSFLS) ../$@/docs
	mv $@/Makefile.all $@/Makefile
	@ echo ''
	@ echo 'Please rename $@ to a version and tar up file'

$(LIBRARY) : $(OBJS)
	$(AR) cr $@ $?
	$(RANLIB) $@

tests :: $(TEST)

$(TEST) : $(TEST).o $(OBJS)
	rm -f $@
	$(CC) $(LDFLAGS) $(TEST).o $(OBJS) $(LIBS)
	mv a.out $@

