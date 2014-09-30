all:
	make -C dropper
	make -C core release
	sleep 5
	make -C build upload
