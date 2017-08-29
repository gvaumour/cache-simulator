/** 
Copyright (C) 2016 Gregory Vaumourin

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/

#include <stdint.h>
#include <assert.h>
#include <string>
#include <zlib.h>
#include <fstream> 
#include <algorithm>

#include "MemoryTrace.hh"
#include "common.hh"

using namespace std;


MemoryTrace::MemoryTrace(string trace) : m_trace(trace), m_isOpen(false)
{
	/*m_isOpen = false;

	if(!m_isOpen){
		cout << "Error -- Trace " << trace << " cannot be opened" << endl;
		exit(0);
	}*/
}



bool 
CompressedTrace::readNext(Access& element)
{
	assert(m_isOpen);
	
	char dummy[TRACE_BUFFER_SIZE];
	string line;
	vector<string> split_line;			

	if(!gzread(m_compressedTrace, dummy, TRACE_BUFFER_SIZE))
		return false;

	line = dummy;
	split_line = split(line , ' ');

	string addr = split_line[0];

	if(addr[0] != '#' || addr[1] != '!')
	{
		cout << "Line passed - Wrong Format" << endl;
		return true;
	}
	
	addr.erase( remove(addr.begin(), addr.end(), '#'), addr.end());
	addr.erase( remove(addr.begin(), addr.end(), '!'), addr.end());

//	cout << "Addr: " << addr << endl;
	
	element.m_address = hexToInt(addr);
	int operation = atoi(split_line[1].c_str());		
	element.m_size = atoi(split_line[2].c_str());		
	element.m_idthread = atoi(split_line[3].c_str());
	element.m_pc = hexToInt(split_line[4]);


	switch(operation){
		case 0 : element.m_type = MemCmd::INST_READ;break;
		case 1 : element.m_type = MemCmd::DATA_READ;break;
		case 2 : element.m_type = MemCmd::DATA_WRITE;break;
		default: assert(false && "operation not recognized \n");
	}

	return true;
}

bool
CompressedTrace::open()
{
	m_compressedTrace = gzopen(m_trace.c_str() , "rb8");
	if(!m_compressedTrace)
		cout << "The trace " << m_trace << " cannot be opened" << endl;
	else
		m_isOpen = true;

	return m_isOpen;
}

void
CompressedTrace::close()
{
	gzclose(m_compressedTrace);	
	m_isOpen = false;
}



bool
TextTrace::readNext(Access& element)
{	
	assert(m_isOpen);

	string line;
	vector<string> split_line;
			
	if(!getline(m_textTrace , line)){
		return false;
	}
	
	if(line.empty())
		return false;
	
	split_line = split(line , ' ');

	string addr = split_line[0];

	if(addr[0] != '#' || addr[1] != '!')
	{
		cout << "Line passed - Wrong Format" << endl;
		return false;
	}

	addr.erase( remove(addr.begin(), addr.end(), '#'), addr.end());
	addr.erase( remove(addr.begin(), addr.end(), '!'), addr.end());

	
	element.m_address = hexToInt(addr);
	int operation = atoi(split_line[1].c_str());		
	element.m_size = atoi(split_line[2].c_str());		
	element.m_idthread = atoi(split_line[3].c_str());
	element.m_pc = hexToInt(split_line[4]);


	switch(operation){
		case 0 : element.m_type = MemCmd::INST_READ;break;
		case 1 : element.m_type = MemCmd::DATA_READ;break;
		case 2 : element.m_type = MemCmd::DATA_WRITE;break;
		default: assert(false && "operation not recognized \n");
	}

	return true;
}

bool
TextTrace::open()
{
	m_textTrace.open(m_trace);
	if(!m_textTrace)
		cout << "The trace " << m_trace << " cannot be opened" << endl;
	else
		m_isOpen = true;


	return m_isOpen;
}


void
TextTrace::close()
{
	m_textTrace.close();
}


MemoryTrace* initTrace(std::string trace_name)
{
	std::vector<std::string> split_line;
	std::string ext = split(trace_name , '.').back();	
	if(ext.size() == 3 && ext == "txt")
		return new TextTrace(trace_name);
	else
		return new CompressedTrace(trace_name);		

}

