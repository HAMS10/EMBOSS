#!/usr/bin/perl -w

%databases = (
              "embl" => "embl embl embl acc [.]dat\$",
	      "genbank" => "genbank genbank gb acc [.]seq\$",
	      "swiss" => "swiss sw sw acc [.]dat\$",
              "pir" => "pir pir pir id [.]ref\$",
	      "swnew" => "swnew swnew sw acc [.]dat\$"
	      );


foreach $d (sort( keys(%databases))) {
    my ($dir, $db,$fmt,$field, $fname) = split (/ /, $databases{$d});
    my $srsdb;
    print "$d: '$dir' '$db' '$fmt' '$field' '$fname'\n";
    opendir(DIR, $dir);
    while($file = readdir(DIR)) {
	if($file !~ $fname) {next}
	print ("\n  $file\n");
	open(IN, "$dir/$file") || die "Cannot open $dir/$file";
	$ac=0;
	$allok = 1;
	$change=0;
	if(-e "newfile") {system "rm newfile;touch newfile";}
	if($fmt eq "pir") {
	    if(-e "newfile2") {system "rm newfile2;touch newfile2";}
	}
	if($file eq "est.dat") {$srsdb = $db."est"} 
	elsif($file eq "condiv.dat") {$srsdb = $db."con"} 
	elsif($file eq "wgs.dat") {$srsdb = $db."wgs"} 
	else {$srsdb = $db}
	while (<IN>) {
	    $id="";
	    if($fmt eq "embl") {
		if(/^AC +([^ ;\n]+)/) {
		    if(!$ac++){$id = $1}}
		else {$ac=0}
	    }
	    elsif($fmt eq "sw") {
		if(/^AC +([^ ;\n]+)/) {if(!$ac++){$id = $1}}
		else {$ac=0}
	    }
	    elsif($fmt eq "gb") {
		if(/^ACCESSION +([^ ;\n]+)/) {$id = $1}
	    }
	    elsif($fmt eq "pir") {
		if(/^>..;([^ ;\n]+)/) {$id = $1}
	    }

	    if($id eq "") {next}

	    print "  id:$id";
	    $istatus = system "EMBOSSRC=./dbfetch/ ;export EMBOSSRC ;EMBOSS_RCHOME=N ;export EMBOSS_RCHOME ;entret  -auto  t$db\-$field:$id x.x";
	    $jstatus = system "EMBOSSRC=./dbfetch ;export EMBOSSRC ;EMBOSS_RCHOME=N ;export EMBOSS_RCHOME ;entret -auto srs$srsdb\-$field:$id y.y";
	    if(!$jstatus && $fmt eq "pir") {
		$file2 = $file;
		$file2 =~ s/[.]ref/.seq/g;
		open (Y, "y.y");
		open (W, ">w.w");
		my $pirseq = "";
		my $pircount = 0;
		my $pirlines = 0;
		my $isseq = 0;
		while(<Y>) {
		    print W;
		    if(/^>/) {$pircount++;$pirlines=0}
		    else {$pirlines++}
		    if($pircount == 2){
			if($pirlines < 2) {next}
			$isseq = 1;
			s/\s//g;
		    }
		    $pirseq .= $_;
		}
		if($isseq) {$pirseq .= "\n"}
		close Y;
		close W;
		open (Y, ">y.y");
		print Y $pirseq;
		close Y;
	    }
	    system "diff x.x y.y > z.z";
	    if($istatus) {
		print " current failed $istatus\n"; 
		$allok = 0;
	    }
	    elsif($jstatus) {
		print " latest failed $jstatus\n";
		$allok = 0;
	    }
	    elsif(-s "z.z") {
		printf " diff %d %d %d\n", (-s "z.z"), (-s "x.x"), (-s "y.y");
		$change=1;
		if($fmt eq "pir") {
		    open (Z, "z.z");
		    while(<Z>) {print}
		    close Z;
		    open (REF, ">>newfile");
		    open (SEQ, ">>newfile2");
		    open (Y, "y.y");
		    $ref = 0;
		    $seq=0;
		    @pirseq = ();
		    while(<Y>) {
			if(/^>/) {
			    $ref++;
			    if($ref == 1) {$saveref = $_}
			    if($ref == 2) {$_ = $saveref}
			}
			if($ref < 2) {
			    print REF;
			}
			else {
			    $seq++;
			    if($seq <= 2) {
				print SEQ;
			    }
			    else {
				chomp;
				push @pirseq,  split (/\s+/, $_);
			    }
			}
		    }
		    foreach $p (@pirseq) {
#			if($j++ == 8) {
#			    $j=0;
#			    print SEQ "\n";
#			}
			print SEQ "$p";
		    }
		    print SEQ "\n";
		    close REF;
		    close SEQ;
		    close Y;
		}
		else {
		    system "cat y.y >> newfile";
		}
	    }
	    else {
		print " OK\n";
		if($fmt eq "pir") {
		    open (REF, ">>newfile");
		    open (SEQ, ">>newfile2");
		    open (Y, "w.w");
		    $ref = 0;
		    $seq=0;
		    @pirseq = ();
		    while(<Y>) {
			if(/^>/) {
			    $ref++;
			    if($ref == 1) {$saveref = $_}
			    if($ref == 2) {$_ = $saveref}
			}
			if($ref < 2) {
			    print REF;
			}
			else {
			    $seq++;
			    if($seq <= 2) {
				print SEQ;
			    }
			    else {
				push @pirseq,  split (/\s+/, $_);
			    }
			}
		    }
		    foreach $p (@pirseq) {
			print SEQ "$p";
		    }
		    print SEQ "\n";
		    close REF;
		    close SEQ;
		    close Y;
		}
		else {
		    system "cat y.y >> newfile";
		}
	    }
	}
	close(IN);
	if($allok && $change) {
	    $replace++;
	    printf "REPLACE $dir/$file %d %d\n", (-s "$dir/$file"), (-s "newfile");
	    system "cp newfile $dir/$file";
	    if($fmt eq "pir") {
		printf "REPLACE $dir/$file2 %d %d\n", (-s "$dir/$file2"), (-s "newfile2");
		system "cp newfile2 $dir/$file2";
	    }
	}
	elsif($allok) {
	    system "diff $dir/$file newfile > z.z";
	    if(-s "z.z") {
		printf "KEEP but size changed %d %d %d\n",
		(-s "$db/$file"), (-s "newfile"), (-s "z.z");
	    }
	    else {
		print "KEEP unchanged\n";
	    }
	    if($fmt eq "pir") {
		system "diff $dir/$file2 newfile2 > z.z";
		if(-s "z.z") {
		    $replace++;
		    printf "KEEP but REF size changed %d %d %d\n",
		    (-s "$db/$file"), (-s "newfile"), (-s "z.z");
		}
	    }

	}
	else {
	    print "PROBLEM somewhere\n";
	}
    }
}

if($replace) {
    print "Replaced $replace databases\n";
}
else {
    print "ALL UNCHANGED\n";
}
