# Author: Robert Crocombe
#

CC = gcc
CXX = g++

# -O2 optimize a bit: usually -O3 is not so great for gcc
# -Wall turn on all warnings
# -MMD autobuild dependencies: leave out system headers
# -MF the name of the dependency file to use.
COMMON_FLAGS = -DDEBUG_ON=$(DEBUG_ON) -march=athlon -O2 -Wall -fPIC
CCFLAGS = $(COMMON_FLAGS)
CXXFLAGS = $(COMMON_FLAGS)
LIBRARIES = -lpthread

DEBUG_ON=0

SOURCE_DIR = ./source
INCLUDE_DIR = ./include

INCLUDES = -I$(INCLUDE_DIR)

# What we're generating


MAIN_SOURCE = $(SOURCE_DIR)/utility.cpp \
	      $(SOURCE_DIR)/scheduler_utils.cpp \
	      $(SOURCE_DIR)/pthread_nap.cpp \
	      $(SOURCE_DIR)/timing.cpp \
	      $(SOURCE_DIR)/random_utilities.cpp \
	      $(SOURCE_DIR)/cpuset_manager.cpp \
	      $(SOURCE_DIR)/cpuset.cpp

CXX_SOURCE = $(MAIN_SOURCE)
C_SOURCE =

# here's what we want to make
MAINFILE = libsystemthing.so

# here's how we make it
.SUFFIXES: .cpp .c .o

.c.o:
	$(CC) $(CCFLAGS) $(INCLUDES) -c $*.c -o $*.o
	$(CC) $(CCFLAGS) $(INCLUDES) -MM $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -rf $*.d.tmp
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $*.cpp -o $*.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MM $*.cpp > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -rf $*.d.tmp

# the targets

CXX_OBJECTS = $(CXX_SOURCE:.cpp=.o)
C_OBJECTS = $(C_SOURCE:.c=.o)

OBJECTS = $(CXX_OBJECTS) $(C_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

$(MAINFILE):	$(OBJECTS)
#$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)
		$(CXX) -shared -o $@ $(OBJECTS)

-include $(OBJECTS:.o=.d)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(DEPS)

.PHONY: mrproper
mrproper:
	rm -f $(OBJECTS) $(MAINFILE) $(DEPS)
