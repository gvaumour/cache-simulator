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

#ifndef MEMORYTRACE_HPP
#define MEMORYTRACE_HPP

#include <stdint.h>
#include <string>
#include <fstream>
#include <iostream>
#include <zlib.h>
#include "common.hh"
#include "Cache.hh"

#define TRACE_BUFFER_SIZE 50

/**
	Wrapper for Reading Memory Traces 
	Read either text file or traces compressed with the zlib
	Format for each access is :
	Type @address dataSize <id_thread>
	
	Where Type can be :
		10 for normal read
		11 for normal write 
		20 for RO access 	
	
	If the extension of the file is .txt the text file wrapper
        is used otherwise the compressed wrapper will be used
*/


class MemoryTrace{
	
	public : 
		MemoryTrace(std::string trace);
		MemoryTrace() : m_trace() , m_isOpen(false) {};

		virtual bool readNext(Access& element) = 0;
		virtual bool open() = 0;
		virtual void close() = 0;
		bool isOpen() {return m_isOpen;};
		uint64_t compressPC(uint64_t pc);
		
	protected : 
		std::string m_trace;
		bool m_isOpen;	
};


class CompressedTrace : public MemoryTrace{
	
	public : 
		CompressedTrace(std::string trace) : MemoryTrace(trace) { open();};
		
		bool readNext(Access& element);
		bool open();
		void close();

	protected : 
		gzFile m_compressedTrace;
};

class TextTrace : public MemoryTrace{
	
	public : 
		TextTrace(std::string trace) : MemoryTrace(trace) { open();};
		
		bool readNext(Access& element);
		bool open();
		void close();

	protected : 
		std::ifstream m_textTrace;
		
};

MemoryTrace* initTrace(std::string trace_name);

#endif 
