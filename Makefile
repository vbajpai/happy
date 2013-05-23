all: build

build:
	pandoc INDEX.md -o INDEX.md.html
	pandoc RELEASES.md -o RELEASES.md.html
	pandoc INSTALL-OSX-HOMEBREW.md -o INSTALL-OSX-HOMEBREW.md.html
	pandoc INSTALL-OSX-MACPORTS.md -o INSTALL-OSX-MACPORTS.md.html
	pandoc INSTALL-DEBIAN.md -o INSTALL-DEBIAN.md.html
	
clean:
	rm -f -r INDEX.md.html RELEASES.md.html INSTALL-OSX-HOMEBREW.md.html .DS_Store
	rm -f -r INSTALL-OSX-MACPORTS.md.html INSTALL-DEBIAN.md.html
