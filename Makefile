CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -g -O0
PROFILEFLAGS= -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DPACODA_VERSION="\"$(VERSION)\""
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
PYMOD       = -shared -fPIC -lpython$(PYVERSION) -Wl,-O1 -Wl,-Bsymbolic-functions -I/usr/include/python$(PYVERSION)
LDPACODA    = -lpacoda -L. -Wl,-rpath,`pwd`

PYVERSION = 2.6

OBJS        = general.o ptransport.o website.o pfildes.o psocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o pcnf.o pderr.o\
			  simclist.o
EXEC_OBJS   = pacoda.o pacoda-website.o pcom-test-client.o pcom-test-server.o

EXECS       = pacoda-proxy pacoda-application pacoda
TEST_EXECS  =
SHARED_OBJS = libpacoda.so
PYMOD_OBJS  = pacoda.so

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
	@#if [ -f /etc/init.d/pacoda ]; then \
	#	echo \\n--- Shutting down running pacoda ---; \
	#	/etc/init.d/pacoda stop; \
	#fi
	@#echo --- Success ---;
	@echo \\n--- Copying pacoda execs, libs, and config files ---;
	cp -f $(EXECS) $(INSTALL_EXECDIR)
	cp -f $(SHARED_OBJS) $(INSTALL_LIBDIR)
	cp -f $(PYMOD_OBJS) $(INSTALL_PYDIR)
	cp -f init.d/pacoda init.d/pacoda-proxy /etc/init.d/
	@echo --- Success ---;
	@echo \\n--- Setting up system to autostart pacoda ---;
	chmod 755 /etc/init.d/pacoda
	chmod 755 /etc/init.d/pacoda-proxy
	update-rc.d pacoda defaults > /dev/null
	update-rc.d pacoda-proxy defaults > /dev/null
	@echo --- Success ---;
	@#echo \\n--- Starting pacoda ---;
	@#/etc/init.d/pacoda start
	@#echo --- Success ---;
	@echo \\nFinished. Installation succeeded!;

##### EXECS #####
#################

pacoda: $(EXEC_DEPENDS) psocket.o ptransport.o pfildes.o pderr.o pcnf.o general.o simclist.o
	$(EXEC_COMPILE) -lyaml

pacoda-proxy: $(EXEC_DEPENDS) general.o pfildes.o website.o psocket.o daemon.o ptransport.o pderr.o
	$(EXEC_COMPILE)

pacoda-application: $(EXEC_DEPENDS) libpacoda.so
	$(EXEC_COMPILE) $(LDPACODA)

##### SHARED OBJS #####

libpacoda.so: $(SHARED_OBJ_DEPENDS) general.o ptransport.o pfildes.o website.o mime.o connection.o\
			 					    headers.o params.o cookies.o urldecoder.o pcnf.o psocket.o pderr.o simclist.o
	$(EXEC_COMPILE) $(SHARED) -lyaml

##### PYTHON MODS #####

pacoda.so: $(PYMOD_DEPENDS) libpacoda.so
	$(EXEC_COMPILE) $(PYMOD) $(LDPACODA)

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
