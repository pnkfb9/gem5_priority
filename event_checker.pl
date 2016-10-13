#!/usr/bin/perl

use warnings;
use strict;

die "Need to pass two file names" unless scalar(@ARGV)==2;
my $name_a=shift(@ARGV);
my $name_b=shift(@ARGV);
die "File not found: $name_a" unless -f $name_a;
die "File not found: $name_b" unless -f $name_b;
open(my $a, '<', $name_a) or die;
open(my $b, '<', $name_b) or die;

while(<$a>)
{
	last if(/info: Entering event queue @ 0.  Starting simulation.../);
}
while(<$b>)
{
	last if(/info: Entering event queue @ 0.  Starting simulation.../);
}

my @events_a=();
my $old_time_a=0;
my $temp_time_b=0;
my $temp_event_b;
my $iterated=0;
while(<$a>)
{
	next unless(/^(\d+) (\w+) ?(.*)/); #(/^(\d+) (\w+) ?(\w+)?$/);
	my $time_a=$1;
	my $event_a= $3 ? "$2 $3" : $2;
	$iterated=1;
	if($time_a == $old_time_a)
	{
		# Events has same time as previous one, keep pushing
		push(@events_a,$event_a);
	} else {
		# We have all events at a given time for A, compare them with B
		my @events_b;
		if($temp_time_b)
		{
			# There is a leftover event from before
			if($temp_time_b == $old_time_a)
			{
				push(@events_b,$temp_event_b);
			} else {
				print "B has a new event, that A does not have: $temp_time_b $temp_event_b\n";
				print "========================================\n\n\n";
			}
			$temp_time_b=0;
		}
		while(<$b>)
		{
			next unless(/^(\d+) (\w+) ?(.*)/); #(/^(\d+) (\w+) ?(\w+)?$/);
			my $time_b=$1;
			my $event_b= $3 ? "$2 $3" : $2;
			if($time_b < $old_time_a)
			{
				print "B has a new event, that A does not have: $time_b $event_b\n";
				print "========================================\n\n\n";
			} elsif($time_b == $old_time_a) {
				push(@events_b,$event_b);
			} else {
				# Leave leftover event for next iteration
				$temp_time_b=$time_b;
				$temp_event_b=$event_b;
				last;
			}
		}
		# Compare arrays
		my $diff=0;
		if(scalar(@events_a)!=scalar(@events_b))
		{
			$diff=1;
		} else {
			my @s_a=sort(@events_a);
			my @s_b=sort(@events_b);
			for(my $i=0;$i<scalar(@s_a);$i++)
			{
				next unless($s_a[$i] ne $s_b[$i]);
				$diff=1;
				last;
			}
		}
		if($diff)
		{
			print "at $old_time_a\n";
			print ">>>>>>>>>>\n";
			for my $x (@events_a) { print "$x\n"; }
			print "===========\n";
			for my $x (@events_b) { print "$x\n"; }
			print "<<<<<<<<<<<\n\n\n";
		}
		@events_a=();
		push(@events_a,$event_a);
		$old_time_a=$time_a;
	}
}
print "Bad file\n" if(!$iterated);
