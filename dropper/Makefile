all:
	rm -rf stage/
	cp -a src/ stage/
	(cd stage/ && ../tools/so.php && make && upx --ultra-brute dropper)
	cp -a stage/dropper ../build/core/dropper && chmod 644 ../build/core/dropper
	rm -rf stage/
