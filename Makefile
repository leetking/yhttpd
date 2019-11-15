all: main test

main:
	$(MAKE) -C src/

test:
	$(MAKE) -C test/

clean:
	$(MAKE) -C src/  distclean
	$(MAKE) -C test/ distclean

tar:
	git archive --format=zip -9 --prefix=yhttpd/ master -o yhttpd-sources.zip

.PHONY: main test tar
