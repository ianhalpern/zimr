CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -O0 -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DBUILD_DATE="\"`date`\"" -DPODORA_VERSION=\"$(VERNUM)\"
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
PYMOD       = -shared -fPIC -lpython$(PYVERSION) -Wl,-O1 -Wl,-Bsymbolic-functions -I/usr/include/python$(PYVERSION)
LDPODORA    = -lpodora -L. -Wl,-rpath,`pwd`

PYVERSION = 2.6

OBJS        = general.o ptransport.o website.o pfildes.o psocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o pcnf.o
EXEC_OBJS   = podora.o podora-website.o pcom-test-client.o pcom-test-server.o

EXECS       = podorapd podora-website podora
TEST_EXECS  =
SHARED_OBJS = libpodora.so
PYMOD_OBJS  = podora.so

SRCDIR     = src
LIB_SRCDIR = $(SRCDIR)/lib
PY_SRCDIR  = $(SRCDIR)/python

VERNUM  = `vernum`

OBJ_DEPENDS         = %.o: $(SRCDIR)/%.c $(SRCDIR)/%.h $(SRCDIR)/config.h
EXEC_DEPENDS        = %: $(SRCDIR)/%/main.c $(SRCDIR)/config.h
SHARED_OBJ_DEPENDS  = lib%.so: $(LIB_SRCDIR)/%.c $(LIB_SRCDIR)/%.h $(SRCDIR)/config.h
PYMOD_DEPENDS       = %.so: $(PY_SRCDIR)/%module.c $(SRCDIR)/config.h

EXEC_COMPILE = $(CC) $(LDFLAGS) $(TARGET_ARCH) $(DSYMBOLS) -I$(SRCDIR) -I$(LIB_SRCDIR) $(OUTPUT) $^
OBJ_COMPILE  = $(CC) $(CFLAGS) $(TARGET_ARCH) $(OUTPUT) $<

##### USER BUILD COMMANDS #####
###############################

make debug beta: $(SHARED_OBJS) $(EXECS) $(PYMOD_OBJS)

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) *.o *.so gmon.out

##### EXECS #####
#################

podora: $(EXEC_DEPENDS) psocket.o ptransport.o pfildes.o
	$(EXEC_COMPILE)

podorapd: $(EXEC_DEPENDS) general.o pfildes.o website.o psocket.o daemon.o ptransport.o
	$(EXEC_COMPILE)

podora-website: $(EXEC_DEPENDS) libpodora.so
	$(EXEC_COMPILE) $(LDPODORA)

##### SHARED OBJS #####

libpodora.so: $(SHARED_OBJ_DEPENDS) general.o ptransport.o pfildes.o website.o mime.o connection.o\
			 					    headers.o params.o cookies.o urldecoder.o daemon.o pcnf.o psocket.o
	$(EXEC_COMPILE) $(SHARED) -lyaml

##### PYTHON MODS #####

podora.so: $(PYMOD_DEPENDS) libpodora.so
	$(EXEC_COMPILE) $(PYMOD) $(LDPODORA)

##### OBJS ######
#################

$(OBJS): $(OBJ_DEPENDS)
	$(OBJ_COMPILE)

####################

##### COMMAND VARIABLE MODS #####
#################################

debug: CC += $(DBFLAGS)
debug: DSYMBOLS += -DDEBUG
tests: CC += $(DBFLAGS)
$(TEST_EXECS): CC += $(DBFLAGS)
debug: VERNUM = `vernum 1`b-debug
beta:  VERNUM = `vernum 1`b
