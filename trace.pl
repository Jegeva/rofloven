#! /usr/bin/perl
use strict;
use warnings;
use IO::Handle;
# Set up the serial port
use Device::SerialPort;

$SIG{INT}=sub {
system("./plot.sh output.csv");
exit;
};

my $port = Device::SerialPort->new("/dev/ttyUSB0");
$|=1;

my $time = time(); 

open(my $fh,("+>output".$time.".csv"));
open(my $fh2,(">output.csv"));
#    or die "cannot open > output".$time.".csv : $!";;

# 19200, 81N on the USB ftdi driver
$port->baudrate(9600); # you may change this value
$port->databits(8); # but not this and the two following
$port->parity("none");
$port->stopbits(1);
my $i=0;



while (1) { 
  my $c = $port->read(1);
  if(length $c){
      if($c ne '\0'){
	  if($c ne '\r'){
	      print $c; 
	      print $fh $c;
	      print $fh2 $c;
	  }
      }
      $fh->flush();
  }
 
}


