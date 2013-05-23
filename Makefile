all: build

build:
	pandoc INDEX.md -o INDEX.md.html
	
clean:
	rm -f -r INDEX.md.html .DS_Store
