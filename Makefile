CC = avr-gcc
CFLAGS = -Os 
CPUFLAGS = -DF_CPU=16000000UL -mmcu=atmega328p
PROG = avrdude 

rofloven.hex : rofloven.bin
	avr-objcopy -O ihex -R .eeprom rofloven.hex
	ls -l rofloven.hex

rofloven.bin : rofloven.o
	$(CC) $(CPUFLAGS) -o rofloven.hex rofloven.o

rofloven.o : rofloven.c
	$(CC) $(CFLAGS) $(CPUFLAGS) -c -o rofloven.o rofloven.c

flash : rofloven.hex
	$(PROG) -F -V -c arduino -p ATMEGA328P -P /dev/ttyUSB0 -b 57600 -D -U flash:w:rofloven.hex:i


clean :
	rm *.hex *.o