#!/usr/bin/perl

use warnings;
use strict;

my $period=80000; #Increasing this (in 2000 steps) increases duty cycle resolution
my $multiplier=20; #How many periods of period change to simulate
for(my $i=2000;$i<$period;$i+=2000)
{
	my $duty=$i/$period;
	print("$duty ");
	open(my $freq, '>', 'freq.txt') or die;
	my $minusduty=$period-$i;
	print $freq ("+duty -duty +period -period\n$i $minusduty 500 2000\n");
	close($freq);
	my $simperiod=$multiplier*$period;
	`build/ALPHA_Network_test_no_cpu/gem5.opt configs/example/ruby_network_test_f.py --clock=1GHz --router-frequency=1GHz --caches --topology Mesh --num-dirs=4 --garnet-network fixed -n 4 --mesh-rows 4 -i 0.1 --synthetic 0 --internal-sampling-freq=1ps --maxtick=$simperiod --sim-cycles=$simperiod > fuffa.txt 2>&1`;
	open(my $st, '<', 'm5out/ruby.stats') or die;
	while(<$st>)
	{
		next unless(/^Total flits received = (\d+)$/);
		print("$1\n");
	}
	close($st);
}
