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
		m = re.search('(\w+)/\w+\_\d\s+(\d+\.\d+)\%', line );
	
		if m != None:
			field1 = m.group(1);
			field2 = float(m.group(2));
			
			if field1 not in result:
				result[field1] = [list()]; 
			
			result[field1][0].append(field2);
			
	
#print(result);		
for k,v in result.items():
	plot = str(k);
	for value in v:
		if value:
			avg = sum(value) / float(len(value))
			plot += "\t" + str(avg) + "%";
	print(plot);
