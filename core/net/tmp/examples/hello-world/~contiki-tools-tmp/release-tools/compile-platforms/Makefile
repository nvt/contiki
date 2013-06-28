all:
	@rm -f *.output
	@echo Compiling on:
	@uname -a
	@$(MAKE) -s -k compile || cat *.output

compile: msp430 native avr arm

msp430: sky.platform esb.platform z1.platform
native: native.platform
avr:    avr-atmega128rfa1.platform avr-raven.platform avr-ravenusb.platform
arm:    econotag.platform

clean:
	@rm -rf [a-z]*

%.platform:
	@mkdir -p $*
	@cp *.c $*
	@cp Makefile.platform $*/Makefile
	@((echo; echo ------------------- $* -------------------; \
           cd $* ; ($(MAKE) TARGET=$* > /dev/null 2>&1 ) ; \
                   $(MAKE) TARGET=$*) > $*.output 2>&1 && echo "$* succeeded") || \
           (echo; echo "$* failed"; exit 1)
