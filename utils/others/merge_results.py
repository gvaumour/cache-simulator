import os
import re
import sys


results={};

def read_file(filename):

	with open(filename) as f:
		for line in f:
			split_line = line.split("\t");		

			if len(split_line) !=2:
				continue;
			
			key, value = split_line[0], float(split_line[1]);

			if key in results:
				results[key].append(value);
			else:
				results[key] = [value];

for a in range(1, len(sys.argv)):
	read_file(sys.argv[a]);


for k,v in results.items():
	sys.stdout.write(k + "\t");
	for a in v:
		sys.stdout.write( str(a) + "\t");
	sys.stdout.write("\n");
	sys.stdout.flush();
