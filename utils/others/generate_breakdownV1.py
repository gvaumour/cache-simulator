import re
import sys

def read_benchname(line, isSpec):
	bench_name = "";
	if isSpec:
		split_line = line.split("\t");
		entete = split_line[0].split("/");
		bench_name = entete[-1];
	else:	
		m = re.search('(\w+)/\w+\_\d' , line);		
		if m != None:
			bench_name = m.group(1);
		
	return bench_name;
	

if len(sys.argv) < 2:
	print("Missing the file");
	exit();	

filename = sys.argv[1];
isSpec = False;
if len(sys.argv) == 3:
	if sys.argv[1] == "-spec2006":
		isSpec = True;
		filename = sys.argv[2];

total_timing = 0;
result={};

with open(filename) as f:

	for line in f:

		split_line = line.split("\t");		
		split_line = split_line[1].split(" ");
		split_line = filter(None,split_line);

		bench_name = read_benchname(line , isSpec);

		if bench_name != None:
			nb_values = len(split_line);
			if bench_name not in result:
				result[bench_name] = list(0 for i in range(0,nb_values)); 
			for i in range(len(result[bench_name])):
				result[bench_name][i] += float(split_line[i]);
			
for k,v in result.items():
	plot = str(k);
	total=0;
	for value in v:
#		plot += "\t" + str(value);
		total += value;
	print(plot + "\t" + str(total));
	
