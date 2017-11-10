
all: main test

main:
	$(MAKE) -C src/

test:
	$(MAKE) -C test/

clean:
	$(MAKE) -C src/  distclean
	$(MAKE) -C test/ distclean

.PHONY: main test
