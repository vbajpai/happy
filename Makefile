all: build make

make: build/Makefile
	(cd .build; make)
	(mkdir -p bin; cp .build/happy bin)

build/Makefile: build
	(cd .build; cmake ..)

build:
	mkdir -p .build

clean:
	rm -f  -r .build
	rm -f  -r bin
