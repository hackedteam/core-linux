all: debug32

debug32:
	rm -rf stage/
	cp -a src/ stage/
	(cd stage/ && ../tools/so.php && make -f Makefile.32 && ../tools/patch.php)
	cp -a stage/core test/
	rm -rf stage/

debug64:
	rm -rf stage/
	cp -a src/ stage/
	(cd stage/ && ../tools/so.php && make -f Makefile.64 && ../tools/patch.php)
	cp -a stage/core test/
	rm -rf stage/

release:
	rm -rf stage/
	cp -a src/ stage/
	(cd stage/ && ../tools/so.php && CFLAGS="" make -f Makefile.32)
	cp -a stage/core ../build/core/core32 && chmod 644 ../build/core/core32
	rm -rf stage/
	cp -a src/ stage/
	(cd stage/ && ../tools/so.php && CFLAGS="" make -f Makefile.64)
	cp -a stage/core ../build/core/core64 && chmod 644 ../build/core/core64
	rm -rf stage/
