/*  Author: Aditya Gaur, Kartik Lakhotia
    Usage: ties_parallel inputFilename outputFilename samplingRatio
    About: Code to create a sample graph from an input graph with final number of nodes based on a sampling ratio.
           Algorithm used is TIES (Totally induced edge sampling), based on https://docs.lib.purdue.edu/cgi/viewcontent.cgi?article=2743&context=cstech
    Compile with: g++ -std=c++11 -fopenmp ties_parallel.cpp -o ties_parallel.o
*/
#include<iostream>
#include<fstream>
#include<omp.h>
#include<vector>
#include<random>
#include<string>
#include<cstdio>
#include<ctime>
#include<utility>

//Toggle for verbose comments
#define vbs(x)
//Number of threads to use
int NUM_THREADS=8;
//Number of additions in a sampling thread, after which we make an update to the total
#define dbg(x)

using namespace std;


#include "utils.h"



Graph g;                //csr version of graph
bool *vExist;           //set to true for each vertex which is sampled
int *Vs;                //Array of sampled vertices
edge *Es;               //Array of induced edges
int UPDATE_COUNTER=40;   //batch size for dynamic work allocation

pair<edge*,int> sampleGraph(vector<edge>&, double, int);            //sample original graph
int sampleEdges(vector<edge>&, vector<int>&, bool*,int, int, int, int); //sample edges within a thread
void parseCommandlineArgs(int,char* [], string&, string&, double&); //parse input parameters from command line

int main(int argc, char* argv[]) {

    //Parameters to be specified, can be taken from commandline using parseCommandlineArgs
    string inFilename;
    string outFilename;
    double fi;

    //Working variables
    vector<edge> inputEdgeList; //extracted list of edges from input file
    edge *sampledEdgeList;      //sampled output list of edges from sampling the graph
    int sampledSize;            //holds size of sampled subgraph
    int n,m;                    //number of vertices and edges in original graph
    pair<edge*,int> sampled;    //holds return edge list and list size

    //Timekeeping
    double start,stop;

    parseCommandlineArgs(argc, argv, inFilename, outFilename, fi);  //read input file name, ouput file name and value of fi(sampling ratio)


    vbs(printf("Getting input\n");)

    n = inputGraph(inFilename,inputEdgeList, &g);   //read edge list from input file, returns number of vertices
    m = inputEdgeList.size();                   //number of edges in original graph

    vbs(printf("Input succeeded, graph has %ld vertices and %ld edges\n",n,m);)

    vExist = new bool[n]();
    Vs = new int[n];
    Es = new edge[m];

    dbg(printf("n=%ld m=%ld\n",n,m);)
    //for(int i=0; i<5; ++i){
           sampled = sampleGraph(inputEdgeList,fi,n);  //sample the graph using fi
    //}
    writeGraph(outFilename, sampled.first, sampled.second); 

    return 0;
}

template<typename T> static inline bool updateCounter(unsigned int &counter, T &index, T *globalCounter, T maxSize, uint32_t increment) {
    counter++;
    if (counter >= increment) {
        index = __sync_fetch_and_add(globalCounter, increment);
        counter = 0;
    }
    return false;
}

pair<edge*,int> sampleGraph(vector<edge> &edgeList, double fi, int n){
    double start,middle,end;
    start=omp_get_wtime();


    int m = edgeList.size();                    //number of edges in original graph

    int totalNodes=0;                               //Number of sampled vertices
    vector<int> tmpNodes[NUM_THREADS];              //individual vectors for each thread
    int* tmpNodeSizes = new int [NUM_THREADS]();


    int VsSize = 0;

    int currentVsize = 0;
    int requiredVsize = fi*n;
    int BS = UPDATE_COUNTER*64;
    //Sample edges in parallel
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for
        for(int i=0; i<NUM_THREADS; ++i) {
            thread_local default_random_engine generator(time(0)*omp_get_thread_num());
            thread_local uniform_int_distribution<int> distribution(0,m-1);
            int size, offset;
            int rnd;
            int counter=0;
            int currentOld = __sync_fetch_and_add(&currentVsize,BS);
            while(true){
                rnd = distribution(generator);
                int u = edgeList[rnd].u;
                int v = edgeList[rnd].v;

                if(__sync_bool_compare_and_swap(vExist+u,false,true)){    
                    ++counter;
                    Vs[currentOld++] = u;
                    if(counter >= BS){
                        currentOld = __sync_fetch_and_add(&currentVsize,BS);
                        counter = 0;
                    }
                    if(currentOld >= requiredVsize){
                        break;
                    }
                }

           
                if(__sync_bool_compare_and_swap(vExist+v,false,true)){    
                    ++counter;
                    Vs[currentOld++] = v;
                    if(counter >= BS){
                        currentOld = __sync_fetch_and_add(&currentVsize,BS);
                        counter = 0;
                    }
                    if(currentOld >= requiredVsize){
                        break;
                    }
                }
            }
        }
    }

    VsSize = requiredVsize;

    middle = omp_get_wtime();

    //Induce edges in parallel
    unsigned int EsSize = 0;
    BS = UPDATE_COUNTER*256;
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        thread_local unsigned int myCounter = 0;
        thread_local unsigned int index = __sync_fetch_and_add(&EsSize,BS);
        #pragma omp for schedule (dynamic, UPDATE_COUNTER)
        for (int i=0; i<VsSize; i++)
        {
            int curV = Vs[i];
            int csrStart = g.VI[curV];
            int csrStop  = g.VI[curV+1];
            for(int j=csrStart; j < csrStop; ++j){
                int desV = g.EI[j];
                if(vExist[desV]) {
                    int eId = index + myCounter;
                    if (eId > g.numEdges)
                        break;
                    Es[eId].u = curV;
                    Es[eId].v = desV;
                    updateCounter<unsigned int>(myCounter, index, &EsSize, 0, BS);
                }
            }
        }
        //fill in the holes with sentinel -1 edges
        for (;myCounter<BS; myCounter++) {
            int eId = index + myCounter;
            if (eId > g.numEdges)
                break;
            Es[eId].u = -1;
            Es[eId].v = -1;
        }
    }


    printf("VsSize=%ld EsSize=%ld\n",VsSize,EsSize);
    


    //reset vExist
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic)
        for(int j=0; j<VsSize; ++j){
            vExist[Vs[j]] = false;
        }
    }
    
    end = omp_get_wtime();
    printf("%lf, %lf, %lf\n",(middle-start)*1000,(end-middle)*1000,(end-start)*1000);

    return pair<edge*,int>(Es,EsSize);
}

int sampleEdges(vector<edge> &el, vector<int> &tmpNodes, bool* vExist, int m, int n, int k, int pnum){
    //default_random_engine generator((unsigned int)omp_get_thread_num());
    thread_local default_random_engine generator(time(0)*omp_get_thread_num());
    thread_local uniform_int_distribution<int> distribution(0, m-1);
    int rnd;

    int sum = 0;
    for(int i=0; i<ceil((double)k/(2*NUM_THREADS)); ++i){
        rnd = distribution(generator);
        int u = el[rnd].u;
        int v = el[rnd].v;
        if(__sync_bool_compare_and_swap(&vExist[u], false, true))
        {
            sum++;
            tmpNodes.push_back(u);
        }
        if(__sync_bool_compare_and_swap(&vExist[v], false, true))
        {
            sum++;
            tmpNodes.push_back(v);
        }
    }
    return sum;
}


void parseCommandlineArgs(int argc, char* argv[], string &in, string &out, double &fi){
    if(argc < 4){
        cout<<"Usage: ./static inputFilename outputFilename samplingRatio [numThreads]\n";
        exit(1);
    }
    in = argv[1];
    out = argv[2];
    fi = atof(argv[3]);
    if (argc>4) 
    {
        NUM_THREADS=atoi(argv[4]);
        UPDATE_COUNTER = NUM_THREADS;
    }
}
