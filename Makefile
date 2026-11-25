# A plain makefile for GNU make with gcc or clang (Linux, macOS, MinGW).
# CMake is the main build (see README.md); this is for when CMake isn't wanted.
#
#   make              static libraries + tools
#   make shared       shared libraries (libtfmxreplay.so etc.)
#   make CC=clang CXX=clang++

CC ?= cc
CXX ?= c++
CFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17
CFLAGS += -std=c99
AR ?= ar

LIBS = tfmxreplay soundmonreplay hippelreplay
TFMX = $(wildcard tfmxreplay/src/*.cpp) common/output.cpp
SOUNDMON = $(wildcard soundmonreplay/src/*.cpp) common/output.cpp common/paula.cpp
HIPPEL = $(wildcard hippelreplay/src/*.cpp) common/output.cpp common/paula.cpp
TOOLS = tfmxrender tfmxtrace smonrender smontrace hippelrender hippeltrace

all: $(LIBS:%=lib%.a) $(TOOLS)

libtfmxreplay.a: $(TFMX:.cpp=.o)
libsoundmonreplay.a: $(SOUNDMON:.cpp=.o)
libhippelreplay.a: $(HIPPEL:.cpp=.o)
%.a:
	$(AR) rcs $@ $^

shared: libtfmxreplay.so libsoundmonreplay.so libhippelreplay.so
libtfmxreplay.so: $(TFMX)
libtfmxreplay.so: CXXFLAGS += -DTFMX_SHARED
libsoundmonreplay.so: $(SOUNDMON)
libsoundmonreplay.so: CXXFLAGS += -DSMON_SHARED
libhippelreplay.so: $(HIPPEL)
libhippelreplay.so: CXXFLAGS += -DHIPPEL_SHARED
%.so:
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden -Icommon -I$(dir $(<D))include -I$(<D) -shared -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Icommon -I$(dir $(<D))include -I$(<D) -c -o $@ $<

# The render tools are C; linking with the C++ compiler brings in the C++ runtime the libraries need.
%render.o: %render.c
	$(CC) $(CFLAGS) -Icommon -I$(dir $(<D))include -c -o $@ $<

tfmxrender: tfmxreplay/tools/tfmxrender.o libtfmxreplay.a
smonrender: soundmonreplay/tools/smonrender.o libsoundmonreplay.a
hippelrender: hippelreplay/tools/hippelrender.o libhippelreplay.a
tfmxrender smonrender hippelrender:
	$(CXX) -o $@ $^ -lm

tfmxtrace: tfmxreplay/tools/tfmxtrace.cpp libtfmxreplay.a
smontrace: soundmonreplay/tools/smontrace.cpp libsoundmonreplay.a
hippeltrace: hippelreplay/tools/hippeltrace.cpp libhippelreplay.a
tfmxtrace smontrace hippeltrace:
	$(CXX) $(CXXFLAGS) -Icommon -I$(dir $(<D))include -I$(dir $(<D))src -o $@ $^

clean:
	rm -f $(TFMX:.cpp=.o) $(SOUNDMON:.cpp=.o) $(HIPPEL:.cpp=.o) */tools/*.o $(LIBS:%=lib%.a) $(LIBS:%=lib%.so) $(TOOLS)

.PHONY: all shared clean
