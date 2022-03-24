ifeq (, $(shell which jemalloc-config))
JEMALLOC =
else
JEMALLOCLD = $(shell jemalloc-config --libdir)
JEMALLOC = -L$(JEMALLOCLD) -ljemalloc
endif

OPT = -O3
#OPT = -g

CONCEPTS = -fconcepts -DCONCEPTS
CFLAGS = -DEDGELONG -DDEL -mcx16 $(OPT) -ldl -std=c++17 -march=native -Wall -Wno-subobject-linkage -DUSEMALLOC #-DNDEBUG

OMPFLAGS = -DOPENMP -fopenmp
HGFLAGS = -DHOMEGROWN -pthread
CILKFLAGS = -DCILK -fcilkplus

#g++ -DB32  -march=native typekv.cpp aspen_test.cpp onestr.cpp -Iinclude/ -I/home/pkumar/src/aspen/ -fopenmp -ltbb -o gbench
all: gbench
gbench: aspen_test.cpp Makefile
	g++ -DB32  $(CFLAGS) $(CILKFLAGS) aspen_test.cpp onestr.cpp -Iinclude/ -I/home/pkumar/src/aspen/ -fopenmp -ltbb -o gbench

clean:
	rm gbench
