#!/usr/bin/python

import sys


def main():

	if len(sys.argv) < 6:
		print "You need 6 arguments to launch the script"
	else:
	
		print '#ifndef _RAP_CONFIG_HH_';
		print '#define _RAP_CONFIG_HH_';

		if sys.argv[1] == "true" or sys.argv[1] == "false":
			print '#define ENABLE_BYPASS ' , str(sys.argv[1]);
		else:
			print "ENABLE_BYPASS error ";
			return;

		print '#define RAP_DEAD_COUNTER_SATURATION ' , str(sys.argv[2]);
		print '#define RAP_LEARNING_THRESHOLD ' , str(sys.argv[3]);
		print '#define RAP_WINDOW_SIZE ' , str(sys.argv[4]);
		print '#define RAP_INACURACY_TH ' , str(sys.argv[5]);

		if len(sys.argv) >= 6:
			print '#define ENABLE_LAZY_MIGRATION ' , str(sys.argv[6]);
		else:
			print '#define ENABLE_LAZY_MIGRATION true';		

		print '#endif';

if __name__ == "__main__":
	main();
