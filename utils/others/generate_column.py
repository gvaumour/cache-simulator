import os
import re
import sys

width, height = 257, 60
Matrix = [[0 for x in range(height)] for y in range(width)] 

x,y = 0,0;

with open(sys.argv[1]) as f:
	for line in f:
		if line == "\n":
			x+=1;
			y=0;
			continue;
		
		split_line = line.split("\t");		

		if len(split_line) !=2:
			continue;
#		print line;
#		print "x = " + str(x) + " y = " + str(y);	
		Matrix[x][y] = float(split_line[1]);
		y+=1
		

for y in range(height):
	for x in range(width):
		sys.stdout.write( str(Matrix[x][y]) + "\t");
	sys.stdout.write("\n");
	sys.stdout.flush();
