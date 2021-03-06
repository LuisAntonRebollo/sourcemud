#!/usr/bin/perl
open(OUT, ">src/generated/commands.cc");

print OUT "// AUTOMATICALLY GENERATED - DO NOT EDIT\n";
print OUT "// Generated on $(date)\n";
		
print OUT "#include \"common.h\"\n";
print OUT "#include \"mud/command.h\"\n";
print OUT "int _MCommand::initialize () {\n";
print OUT "AccessID ACCESS_ALL;\n";
print OUT "AccessID ACCESS_GM = AccessID::create(\"gm\");\n";
print OUT "AccessID ACCESS_BUILDER = AccessID::create(\"builder\");\n";
print OUT "AccessID ACCESS_ADMIN = AccessID::create(\"admin\");\n";

for my $file (glob "src/cmd/*.cc") {
	open(IN, "<$file");
	while (<IN>) {
		if (/BEGIN COMMAND/) {
			my $name;
			my $access;
			my $type;
			my @usage;
			my @format;

			while (<IN>) {
				last if /END COMMAND/;

				chomp;
				/name:\s*(.*)/ and $name=$1;
				/access:\s*(.*)/ and $access=$1;
				/usage:\s*(.*)/ and push @usage, $1;
				/format:\s*(.*)/ and push @format, $1;
			}

			while (<IN>) {
				if (/^void command_\w+\s*[(](\w+)/) {
					$type = $1;
					last;
				}
			}

			my $cmd = $name;
			$cmd =~ tr/a-z/_/c;

			push @usage, $name unless @usage;
			push @format, $name unless @format;
			$access="ALL" unless $access;

			die "$file has no name" unless $name ne "";
			die "$file has no type" unless $type ne "";

			print OUT "\n// $file\nCOMMAND(\n";
			print OUT "\t\"$name\",\n";
			print OUT "\t\"", join("\\n\"\n\t\"", @usage), "\\n\",\n";
			print OUT "\tcommand_$cmd,\n";
			print OUT "\tACCESS_$access,\n";
			print OUT "\t$type)\n";

			foreach my $format (@format) {
				my $priority = 50;
				if ($format =~ /^(.*)\s+[(](\d+)[)]\s*$/) {
					$format = $1;
					$priority = $2;
					chomp $format;
				}
				print OUT "\tFORMAT($priority, \"$format\")\n";
			}

			print OUT "END_COMM\n";
		}
	}
	close(IN);
}

print OUT "\nreturn 0;\n";
print OUT "}\n";
