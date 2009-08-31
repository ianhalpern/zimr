CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -g -O0
PROFILEFLAGS= -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DZIMR_VERSION="\"$(VERSION)\""
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
PYMOD       = -shared -fPIC -lpython$(PYVERSION) -Wl,-O1 -Wl,-Bsymbolic-functions -I/usr/include/python$(PYVERSION)
LDZIMR    = -lzimr -L. -Wl,-rpath,`pwd`

PYVERSION = 2.6

OBJS        = general.o ztransport.o website.o zfildes.o zsocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o zcnf.o zerr.o\
			  simclist.o
EXEC_OBJS   = zimr.o zimr-website.o pcom-test-client.o pcom-test-server.o

EXECS       = zimr-proxy zimr-application zimr
TEST_EXECS  =
SHARED_OBJS = libzimr.so
PYMOD_OBJS  = zimr.so

SRCDIR     = src
LIB_SRCDIR = $(SRCDIR)/lib
PY_SRCDIR  = $(SRCDIR)/python

INSTALL_EXECDIR = /usr/bin
INSTALL_LIBDIR  = /usr/lib
INSTALL_PYDIR   = /usr/lib/python$(PYVERSION)

VERSION  = `vernum`

OBJ_DEPENDS         = %.o: $(SRCDIR)/%.c $(SRCDIR)/%.h $(SRCDIR)/config.h
EXEC_DEPENDS        = %: $(SRCDIR)/%/main.c $(SRCDIR)/config.h
SHARED_OBJ_DEPENDS  = lib%.so: $(LIB_SRCDIR)/%.c $(LIB_SRCDIR)/%.h $(SRCDIR)/config.h
PYMOD_DEPENDS       = %.so: $(PY_SRCDIR)/%module.c $(SRCDIR)/config.h

EXEC_COMPILE = $(CC) $(LDFLAGS) $(TARGET_ARCH) $(DSYMBOLS) -I$(SRCDIR) -I$(LIB_SRCDIR) $(OUTPUT) $^
OBJ_COMPILE  = $(CC) $(CFLAGS) $(TARGET_ARCH) $(OUTPUT) $<

##### TOP-LEVEL COMMANDS #####
###############################
.PHONY: make debug profile-debug tests clean install

make debug profile-debug: $(SHARED_OBJS) $(EXECS) $(PYMOD_OBJS)

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) *.o *.so gmon.out

install:
	@#if [ -f /etc/init.d/zimr ]; then \
	#	echo \\n--- Shutting down running zimr ---; \
	#	/etc/init.d/zimr stop; \
	#fi
	@#echo --- Success ---;
	@echo \\n--- Copying zimr execs, libs, and config files ---;
	cp -f $(EXECS) $(INSTALL_EXECDIR)
	cp -f $(SHARED_OBJS) $(INSTALL_LIBDIR)
	cp -f $(PYMOD_OBJS) $(INSTALL_PYDIR)
	cp -f init.d/zimr init.d/zimr-proxy /etc/init.d/
	@echo --- Success ---;
	@echo \\n--- Setting up system to autostart zimr ---;
	chmod 755 /etc/init.d/zimr
	chmod 755 /etc/init.d/zimr-proxy
	update-rc.d zimr defaults > /dev/null
	update-rc.d zimr-proxy defaults > /dev/null
	@echo --- Success ---;
	@#echo \\n--- Starting zimr ---;
	@#/etc/init.d/zimr start
	@#echo --- Success ---;
	@echo \\nFinished. Installation succeeded!;

##### EXECS #####
#################

zimr: $(EXEC_DEPENDS) zsocket.o ztransport.o zfildes.o zerr.o zcnf.o general.o simclist.o
	$(EXEC_COMPILE) -lyaml

zimr-proxy: $(EXEC_DEPENDS) general.o zfildes.o website.o zsocket.o daemon.o ztransport.o zerr.o simclist.o
	$(EXEC_COMPILE)

zimr-application: $(EXEC_DEPENDS) libzimr.so
	$(EXEC_COMPILE) $(LDZIMR)

##### SHARED OBJS #####

libzimr.so: $(SHARED_OBJ_DEPENDS) general.o ztransport.o zfildes.o website.o mime.o connection.o\
			 					    headers.o params.o cookies.o urldecoder.o zcnf.o zsocket.o zerr.o simclist.o
	$(EXEC_COMPILE) $(SHARED) -lyaml

##### PYTHON MODS #####

zimr.so: $(PYMOD_DEPENDS) libzimr.so
	$(EXEC_COMPILE) $(PYMOD) $(LDZIMR)

##### OBJS ######
#################

$(OBJS): $(OBJ_DEPENDS)
	$(OBJ_COMPILE)

####################

##### COMMAND VARIABLE MODS #####
#################################

$(TEST_EXECS ) tests profile-debug debug: CC += $(DBFLAGS)
profile-debug debug: DSYMBOLS += -DDEBUG
profile-debug debug: VERSION := $(VERSION)-debug
profile-debug: CC += $(PROFILEFLAGS)
#debug: EXEC_COMPILE += -rdynamic -ldl $(SRCDIR)/sigsegv.c
