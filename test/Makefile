#
# variables
# 
CXXFLAGS = --std=c++14 @warnings.txt -g -ggdb3 -O0 -fno-inline-functions -I.. -DDEBUG
LDFLAGS = -g
ifeq ($(CXX),g++)
  CXXFLAGS += -fno-inline-small-functions
endif

all: test-fast-ecs

test-fast-ecs: test.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test.o: test.cc ../fastecs.hh

lint: 
	clang-tidy ../fastecs.hh "-checks=*,-google-build-using-namespace,-google-readability-todo" -- --std=c++14

clean:
	$(RM) test.o test-fast-ecs

.PHONY: clean lint
