Trace Generation
================

This sub-project allows to generate compressed memory traces with the zlib library to feed the cache simulator. INTEL's Pin is used to dynamically instrument the application. Hooks can added in the program to select the region of interest to capture


## Compilation 

The zlib development packages and the INTEL's pin v2.14 application are required for the project. Then the project can simply be compiled with make 
The pin v2.14 can be found there --> https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads

## Running:

```bash 
cd <path to the directory in this sub-project>
pin -ifeellucky -injection child -t ./obj-intel64/trace-gen.so -- <the program to instrument>
```

