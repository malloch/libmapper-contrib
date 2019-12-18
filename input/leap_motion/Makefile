OS := $(shell uname)
ARCH := $(shell uname -m)

ifeq ($(OS), Linux)
  ifeq ($(ARCH), x86_64)
    LEAP_LIBRARY := ./lib/x64/libLeap.so -Wl,-rpath,./lib/x64
  else
    LEAP_LIBRARY := ./lib/x86/libLeap.so -Wl,-rpath,./lib/x86
  endif
else
  # OS X
  LEAP_LIBRARY := ./lib/libLeap.dylib
  MPR_LIBRARY := /usr/local/lib/libmapper.dylib
endif

mpr.leap_motion: mpr.leap_motion.cpp
	$(CXX) -Wall -g -I./include mpr.leap_motion.cpp -o mpr.leap_motion $(LEAP_LIBRARY) $(MPR_LIBRARY)
ifeq ($(OS), Darwin)
	install_name_tool -change @loader_path/libLeap.dylib ./lib/libLeap.dylib mpr.leap_motion
endif

clean:
	rm -rf mpr.leap_motion mpr.leap_motion.dSYM
