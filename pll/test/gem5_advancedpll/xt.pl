while(<STDIN>)
{
	if(/%% (\d+) (\d+)/) { print $1, ' ', $2, "\n"; }
}