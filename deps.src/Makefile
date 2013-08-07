
GADEPS		?= $(HOME)/myprog/gaminganywhere/deps.posix
MACHINE		= $(shell uname -m)

TBZUNPACK	= tar xjf
TGZUNPACK	= tar xzf

include Makefile.packages

all: sdl2 sdl2_ttf yasm lame opus ogg vorbis theora vpx x264 xvid ffmpeg live555

sdl2:
	$(TGZUNPACK) $(SDL2)
	mkdir -p $(SDL2DIR)/mybuild
	cd $(SDL2DIR)/mybuild; ../configure --prefix=$(GADEPS) --with-pic
	make -C $(SDL2DIR)/mybuild -j5
	make -C $(SDL2DIR)/mybuild install
	rm -rf $(SDL2DIR)

sdl2_ttf:
	$(TBZUNPACK) $(SDL2_TTF)
	mkdir -p $(SDL2_TTFDIR)/mybuild
	cd $(SDL2_TTFDIR)/mybuild; ../configure --prefix=$(GADEPS) --with-pic
	make -C $(SDL2_TTFDIR)/mybuild -j5
	make -C $(SDL2_TTFDIR)/mybuild install
	rm -rf $(SDL2_TTFDIR)

yasm:
	$(TGZUNPACK) $(YASM)
	cd $(YASMDIR); ./configure --prefix=$(GADEPS)
	make -C $(YASMDIR) -j5
	make -C $(YASMDIR) install
	rm -rf $(YASMDIR)

lame:
	$(TGZUNPACK) $(LAME)
	cd $(LAMEDIR); ./configure --prefix=$(GADEPS) --with-pic
	make -C $(LAMEDIR) -j5
	make -C $(LAMEDIR) install
	rm -rf $(LAMEDIR)

opus:
	$(TGZUNPACK) $(OPUS)
	#
	cd $(OPUSDIR); ./configure --prefix=$(GADEPS) --with-pic
	make -C $(OPUSDIR) -j5
	make -C $(OPUSDIR) install
	rm -rf $(OPUSDIR)

ogg:
	$(TGZUNPACK) $(OGG)
	cd $(OGGDIR); ./configure --prefix=$(GADEPS) --with-pic
	make -C $(OGGDIR) -j5
	make -C $(OGGDIR) install
	rm -rf $(OGGDIR)

vorbis:
	$(TGZUNPACK) $(VORBIS)
	cd $(VORBISDIR); ./configure --prefix=$(GADEPS) --build=$(MACHINE) --with-pic
	make -C $(VORBISDIR) -j5
	make -C $(VORBISDIR) install
	rm -rf $(VORBISDIR)

theora:
	$(TBZUNPACK) $(THEORA)
	cd $(THEORADIR); ./configure --prefix=$(GADEPS) --with-pic
	make -C $(THEORADIR) -j5
	make -C $(THEORADIR) install
	rm -rf $(THEORADIR)

vpx:
	$(TBZUNPACK) $(VPX)
	cd $(VPXDIR); ./configure --prefix=$(GADEPS) --disable-examples --enable-pic
	make -C $(VPXDIR) -j5
	make -C $(VPXDIR) install
	rm -rf $(VPXDIR)

x264:
	$(TBZUNPACK) $(X264)
	cd $(X264DIR); ./configure --prefix=$(GADEPS) --enable-shared --enable-pic
	make -C $(X264DIR) -j5
	make -C $(X264DIR) install
	rm -rf $(X264DIR)

xvid:
	$(TBZUNPACK) $(XVID)
	cd $(XVIDDIR)/build/generic; ./configure --prefix=$(GADEPS) --enable-shared
	make -C $(XVIDDIR)/build/generic -j5
	make -C $(XVIDDIR)/build/generic install
	rm -rf $(XVIDDIR)

ffmpeg:
	$(TBZUNPACK) $(FFMPEG)
	cd $(FFMPEGDIR); ./configure --prefix=$(GADEPS) \
		--enable-pic \
		--enable-libmp3lame \
		--enable-libtheora --enable-libvorbis \
		--enable-libx264 --enable-libvpx \
		--enable-libopus \
		--enable-libxvid \
		--enable-gpl --enable-nonfree --disable-outdev=sdl \
		--extra-cflags=-I$(GADEPS)/include \
		--extra-ldflags=-L$(GADEPS)/lib
	make -C $(FFMPEGDIR) -j5
	make -C $(FFMPEGDIR) install
	rm -rf $(FFMPEGDIR)

live555:
	$(TGZUNPACK) $(LIVE555)
	# enable -fPIC for macosx and linux
	mv -f $(LIVE555DIR)/config.linux $(LIVE555DIR)/config.linux.old
	mv -f $(LIVE555DIR)/config.macosx $(LIVE555DIR)/config.macosx.old
	cat $(LIVE555DIR)/config.linux.old | sed -e 's,^COMPILE_OPTS.*$$,& -fPIC,' > $(LIVE555DIR)/config.linux
	cat $(LIVE555DIR)/config.macosx.old | sed -e 's,^COMPILE_OPTS.*$$,& -fPIC,' > $(LIVE555DIR)/config.macosx
	#
	cd $(LIVE555DIR); ./genMakefiles $(shell uname -s | tr A-Z a-z | sed -e 's,darwin,macosx,' -e 's,cygwin.*,cygwin,')
	make -C $(LIVE555DIR) -j5
	find $(LIVE555DIR) -name '*.a' -exec cp -f {} $(GADEPS)/lib \;
	-mkdir $(GADEPS)/include/live555
	find $(LIVE555DIR) -name '*.hh' -exec cp -f {} $(GADEPS)/include/live555 \;
	cp -f $(LIVE555DIR)/groupsock/include/NetCommon.h $(GADEPS)/include/live555
	rm -rf $(LIVE555DIR)

