#
# variables
# 
CXXFLAGS = --std=c++14 @warnings.txt -g -ggdb3 -O0 -fno-inline-functions -I.. -DDEBUG
LDFLAGS = -g
LDLIBS = -lgtest
ifeq ($(CXX),g++)
  CXXFLAGS += -fno-inline-small-functions
endif

all: test-fast-ecs #example

test-fast-ecs: test.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#example: example.o
#	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test.o: test.cc ../fastecs.hh

#example.o: example.cc ../fastecs.hh

lint:
	clang-tidy ../fastecs.hh "-checks=*,-google-build-using-namespace,-google-readability-todo" -- --std=c++14

clean:
	$(RM) *.o test-fast-ecs

.PHONY: clean lint
