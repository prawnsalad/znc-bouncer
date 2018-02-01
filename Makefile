CXXFLAGS=-O2 --std=c++11 -Wall -Wextra -Wpedantic -Werror -fPIC

all: build

build: bouncer.o
	$(CXX) $(CXXFLAGS) -shared $^ -o bouncer.so
