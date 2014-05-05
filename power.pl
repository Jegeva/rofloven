#! /usr/bin/perl

use warnings;
use strict;
use constant PI    => 4 * atan2(1, 1);

my $x = PI;
my $currx;
my $lasthit=0;
my $target;
my $currval;
my $step;
my $tolerance = "0.0000001";
my $iter = 1;
my $max_val = (('0.5' * $x) - ('0.25' * sin(2*$x)));
my $buckets = 99;
$x = 0;
 print "static int LUT50hz[100]={10000\n";
while ($iter < $buckets){
    $currx=$lasthit;
    $target = ($max_val / ($buckets*'1.0'))*$iter;
    $step = PI;
    $currval = (('0.5' * $currx) - ('0.25' * sin(2*$currx)));
   
    while( $currval > ($target+($tolerance)) || $currval < ($target-($tolerance))   ){
	$currx=$lasthit + $step;
	$currval = (('0.5' * $currx) - ('0.25' * sin(2*$currx)));
	if($currval > ($target+($tolerance))){
	    $step /= 2.0;
	}
	if($currval < ($target-($tolerance))){    
	    $step *= 4.0;
	    $step /= 3.0;
	}
  #  print "$iter:$currval:$step:".($target-($target*$tolerance))."\n";
    }
    print ",".int((1-($currx/PI))*10000)."\n";
    $lasthit = $currx;
    $iter+=1;
}
 print ",0};\n";
