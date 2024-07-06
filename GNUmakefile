CC	=	gcc
TARGET	=	vencode
SRCDIR	=	src
INCLUDE	=	-Iinclude
LIBS	=	-lpng -lpthread
OBJDIR	=	obj
OBJS	=	$(OBJDIR)/args.o \
		$(OBJDIR)/vencode.o \
		$(OBJDIR)/util.o \
		$(OBJDIR)/bmp.o
CFLAGS	=	-Wall -O3 -march=native $(INCLUDE)

$(TARGET) : $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

MINGW_TARGET	=	vencode.exe
MINGW_CC	=	i686-w64-mingw32-gcc
WINDRESS	=	i686-w64-mingw32-windres
MINGW_OBJDIR	=	mingw_obj
RES		=	$(MINGW_OBJDIR)/vencode_private.res
RC		=	MinGW/resources/vencode_private.rc
MINGW_LIBS	=	-lpng -lpthread -lz -fstack-protector -static
MINGW_OBJS	=	$(MINGW_OBJDIR)/args.o \
			$(MINGW_OBJDIR)/vencode.o \
			$(MINGW_OBJDIR)/util.o \
			$(MINGW_OBJDIR)/bmp.o \
			$(RES)
MINGW_SRCLIB	=	MinGW/lib
MINGW_INCLUDE	=	-Iinclude -IMinGW/include
MINGW_CFLAGS	=	-O3 -mtune=i686 -static-libgcc \
			-std=gnu99 $(MINGW_INCLUDE) -Wall
FFMPEG		=	ffmpeg-3.4.1_LTO

mingw: $(MINGW_OBJS) $(FFMPEG)
	$(MINGW_CC) $(MINGW_OBJS) -o $(MINGW_TARGET) -L$(MINGW_SRCLIB) $(MINGW_LIBS)

$(FFMPEG) : MinGW/ffmpeg-3.4.1_LTO.tar.xz
	xz -d < MinGW/ffmpeg-3.4.1_LTO.tar.xz | tar xf -

$(MINGW_OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(MINGW_OBJDIR)
	$(MINGW_CC) -c $(MINGW_CFLAGS) $< -o $@

$(RES): $(RC)
	$(WINDRESS) -i $(RC) -F pe-i386 --input-format=rc -o $(RES) -O coff

.PHONY: clean
clean:
	rm -rf $(OBJDIR)
	rm -f  $(TARGET)
	rm -f  $(MINGW_TARGET) 
	rm -rf $(FFMPEG)
	rm -rf $(MINGW_OBJDIR)

mingw_pack: $(MINGW_TARGET) $(FFMPEG)
	mkdir vencode-`grep V README | tr -d V`/
	cp $(MINGW_TARGET) vencode-`grep V README | tr -d V`/
	cp LICENSE vencode-`grep V README | tr -d V`/
	cp -r ffmpeg-3.4.1_LTO vencode-`grep V README | tr -d V`/
	cp -r MinGW/third-party-licenses vencode-`grep V README | tr -d V`/
	cp misc/LEEME.TXT vencode-`grep V README | tr -d V`/
	zip -rq vencode-`grep V README | tr -d V`.zip \
		vencode-`grep V README | tr -d V`/
	rm -r vencode-`grep V README | tr -d V`
