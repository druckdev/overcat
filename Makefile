NAME = overcat
PREFIX = /usr/local
CFLAGS = -O2 -Wall

.PHONY: all
all: $(NAME) docs

.PHONY: clean
clean:
	rm -f $(NAME)

.PHONY: docs
docs: $(NAME).1

%.1: %.adoc
	a2x -f manpage $<

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(NAME) $(DESTDIR)$(PREFIX)/bin

	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp $(NAME).1 $(DESTDIR)$(PREFIX)/share/man/man1

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(NAME).1
