libecs.so: libecs.cc
	gcc -Wall -Wextra $< -shared -o $@ -fPIC -llua

clean:
	rm libecs.so

check: libecs.so
	lua tests.lua

