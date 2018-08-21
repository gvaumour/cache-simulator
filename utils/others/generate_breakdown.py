import re
import sys

if len(sys.argv) < 2:
	print("Missing the file");
	exit();	

total_timing = 0;
result={};

with open(sys.argv[1]) as f:
#with open("greg") as f:

	for line in f:
		
		if line == "":
			continue;

		split_line = line.split("\t");		
		split_line = split_line[1].split(" ");
		split_line = filter(None,split_line);
		
		m = re.search('(\w+)/\w+\_\d' , line);		
		if m != None:
			field1 = m.group(1);
		
			nb_values = len(split_line);
			if field1 not in result:
				result[field1] = list(0 for i in range(0,nb_values)); 
			
			for i in range(len(result[field1])):
				result[field1][i] += float(split_line[i]);
			
for k,v in result.items():
	plot = str(k);
	total=0;
	for value in v:
		plot += "\t" + str(value);
#		total += value;
	print(plot);
	
