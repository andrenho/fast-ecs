#
# variables
# 
CXXFLAGS = --std=c++17 @warnings.txt -g -ggdb3 -O0 -fno-inline-functions -I.. -DDEBUG
LDFLAGS = -g
ifeq ($(CXX),g++)
  CXXFLAGS += -fno-inline-small-functions
endif

all: test-fast-ecs example

test-fast-ecs: CXXFLAGS += -DTEST
test-fast-ecs: test.o
	$(CXX) $(LDFLAGS) -o $@ $^

example: example.o
	$(CXX) $(LDFLAGS) -o $@ $^

test.o: test.cc ../fastecs.hh

example.o: example.cc ../fastecs.hh

lint:
	clang-tidy ../fastecs.hh "-checks=*,-google-build-using-namespace,-google-readability-todo,-cppcoreguidelines-pro-type-reinterpret-cast,-cppcoreguidelines-pro-type-const-cast,-cert-err52-cpp" -- --std=c++14

check-leaks: test-fast-ecs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=sendmsg.supp ./test-fast-ecs

clean:
	$(RM) *.o test-fast-ecs

.PHONY: clean lint
