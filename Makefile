# A plain makefile for GNU make with gcc or clang (Linux, macOS, MinGW).
# CMake is the main build (see README.md); this is for when CMake isn't wanted.
#
#   make              static libraries + tools
#   make shared       shared libraries (libtfmxreplay.so etc.; libamigareplay.so holds all the replayers)
#   make CC=clang CXX=clang++

CC ?= cc
CXX ?= c++
CFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17
CFLAGS += -std=c99
AR ?= ar
INCLUDES = -Icommon -Itfmxreplay/include -Isoundmonreplay/include -Ihippelreplay/include -Iamigareplay/include

LIBS = tfmxreplay soundmonreplay hippelreplay amigareplay
TFMX = $(wildcard tfmxreplay/src/*.cpp) common/output.cpp
SOUNDMON = $(wildcard soundmonreplay/src/*.cpp) common/output.cpp common/paula.cpp
HIPPEL = $(wildcard hippelreplay/src/*.cpp) common/output.cpp common/paula.cpp
# sort takes out the duplicates
ALL = $(sort $(wildcard amigareplay/src/*.cpp) $(TFMX) $(SOUNDMON) $(HIPPEL))
TOOLS = amigarender tfmxtrace smontrace hippeltrace

all: $(LIBS:%=lib%.a) $(TOOLS)

libtfmxreplay.a: $(TFMX:.cpp=.o)
libsoundmonreplay.a: $(SOUNDMON:.cpp=.o)
libhippelreplay.a: $(HIPPEL:.cpp=.o)
libamigareplay.a: $(ALL:.cpp=.o)
%.a:
	$(AR) rcs $@ $^

shared: $(LIBS:%=lib%.so)
libtfmxreplay.so: $(TFMX)
libtfmxreplay.so: CXXFLAGS += -DTFMX_SHARED
libsoundmonreplay.so: $(SOUNDMON)
libsoundmonreplay.so: CXXFLAGS += -DSMON_SHARED
libhippelreplay.so: $(HIPPEL)
libhippelreplay.so: CXXFLAGS += -DHIPPEL_SHARED
libamigareplay.so: $(ALL)
libamigareplay.so: CXXFLAGS += -DAMIGAREPLAY_SHARED
%.so:
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden $(INCLUDES) -shared -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# The render tool is C; linking with the C++ compiler brings in the C++ runtime the libraries need.
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

amigarender: amigareplay/tools/amigarender.o libamigareplay.a
	$(CXX) -o $@ $^ -lm

tfmxtrace: tfmxreplay/tools/tfmxtrace.cpp libtfmxreplay.a
smontrace: soundmonreplay/tools/smontrace.cpp libsoundmonreplay.a
hippeltrace: hippelreplay/tools/hippeltrace.cpp libhippelreplay.a
tfmxtrace smontrace hippeltrace:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(dir $(<D))src -o $@ $^

clean:
	rm -f $(ALL:.cpp=.o) */tools/*.o $(LIBS:%=lib%.a) $(LIBS:%=lib%.so) $(TOOLS)

.PHONY: all shared clean
