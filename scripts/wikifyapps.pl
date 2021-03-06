#!/usr/bin/perl -w

use English;

#
# Reads the doc/programs/html/index.html file
#
# writes the Appdocs page

$text = "";
while (<>) {
    $text .= $_;
}

print "{|\n";
print "|-\n";
print "!Program name\n";
print "!Description\n";

while($text =~ /<[Tt][Dd][^>]*>.*?<\/[Tt][Dd]>/gosm){
    $r = $MATCH;

    if($r =~ />([^<]+)<\/a>/) {
	$name = $1;
	$cname = ucfirst($name);
	print "|-\n";
	print "|[[Appdoc:$cname|$name]]\n";
	print "|\{\{:Appinc:Short$cname}}\n";
    }
}
print "|}\n";

