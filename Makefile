CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -g -O0 -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DBUILD_DATE="\"`date`\"" -DPORODA_VERSION="\"$(VERSION)\""
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
PYMOD       = -shared -fPIC -lpython$(PYVERSION) -Wl,-O1 -Wl,-Bsymbolic-functions -I/usr/include/python$(PYVERSION)
LDPORODA    = -lporoda -L. -Wl,-rpath,`pwd`

PYVERSION = 2.6

OBJS        = general.o ptransport.o website.o pfildes.o psocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o pcnf.o pderr.o\
			  simclist.o
EXEC_OBJS   = poroda.o poroda-website.o pcom-test-client.o pcom-test-server.o

EXECS       = poroda-proxy poroda-application poroda
TEST_EXECS  =
SHARED_OBJS = libporoda.so
PYMOD_OBJS  = poroda.so

SRCDIR     = src
LIB_SRCDIR = $(SRCDIR)/lib
PY_SRCDIR  = $(SRCDIR)/python

VERSION  = `vernum`

OBJ_DEPENDS         = %.o: $(SRCDIR)/%.c $(SRCDIR)/%.h $(SRCDIR)/config.h
EXEC_DEPENDS        = %: $(SRCDIR)/%/main.c $(SRCDIR)/config.h
SHARED_OBJ_DEPENDS  = lib%.so: $(LIB_SRCDIR)/%.c $(LIB_SRCDIR)/%.h $(SRCDIR)/config.h
PYMOD_DEPENDS       = %.so: $(PY_SRCDIR)/%module.c $(SRCDIR)/config.h

EXEC_COMPILE = $(CC) $(LDFLAGS) $(TARGET_ARCH) $(DSYMBOLS) -I$(SRCDIR) -I$(LIB_SRCDIR) $(OUTPUT) $^
OBJ_COMPILE  = $(CC) $(CFLAGS) $(TARGET_ARCH) $(OUTPUT) $<

##### USER BUILD COMMANDS #####
###############################

make debug: $(SHARED_OBJS) $(EXECS) $(PYMOD_OBJS)

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) *.o *.so gmon.out

##### EXECS #####
#################

poroda: $(EXEC_DEPENDS) psocket.o ptransport.o pfildes.o pderr.o pcnf.o general.o simclist.o
	$(EXEC_COMPILE) -lyaml

poroda-proxy: $(EXEC_DEPENDS) general.o pfildes.o website.o psocket.o daemon.o ptransport.o pderr.o
	$(EXEC_COMPILE)

poroda-application: $(EXEC_DEPENDS) libporoda.so
	$(EXEC_COMPILE) $(LDPORODA)

##### SHARED OBJS #####

libporoda.so: $(SHARED_OBJ_DEPENDS) general.o ptransport.o pfildes.o website.o mime.o connection.o\
			 					    headers.o params.o cookies.o urldecoder.o pcnf.o psocket.o pderr.o simclist.o
	$(EXEC_COMPILE) $(SHARED) -lyaml

##### PYTHON MODS #####

poroda.so: $(PYMOD_DEPENDS) libporoda.so
	$(EXEC_COMPILE) $(PYMOD) $(LDPORODA)

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
debug: VERSION := $(VERSION)-debug
#debug: EXEC_COMPILE += -rdynamic -ldl $(SRCDIR)/sigsegv.c
