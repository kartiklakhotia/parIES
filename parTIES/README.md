##  About the Code
    Generate a sampled subgraph from input graph using parallel edge sampling (both static and dynamic scheduling). For details on edge sampling, refer  https://docs.lib.purdue.edu/cgi/viewcontent.cgi?article=2743&context=cstech 

##  Input
    Graph to be sampled and sampling fraction.
    Format - CSR Binary (use the tool at https://github.com/kartiklakhotia/pcpm/tree/master/csr_gen for generation)

##  Compile
    Can be compiled in 2 ways
1. Command: g++ -std=c++11 -fopenmp static/dynamic.cpp -O3 -o dynamic
2. Run the script "compile.sh"


##  Usage
1. Static - ./static "inputGraphFile" "outputGraphFile" "samplingRatio" "numThreads"(optional)
1. Dynamic - ./dynamic "inputGraphFile" "outputGraphFile" "samplingRatio" "numThreads"(optional)
    


