/*  Author: Aditya Gaur, Kartik Lakhotia
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
//  for(int i=0; i<5; ++i){
        sampled = sampleGraph(inputEdgeList,fi,n);  //sample the graph using fi
//  }
    writeGraph(outFilename, sampled.first, sampled.second); 

    return 0;
}


pair<edge*,int> sampleGraph(vector<edge> &edgeList, double fi, int n){
    double start,middle,end;
    start=omp_get_wtime();


    int m = edgeList.size();                    //number of edges in original graph

    int totalNodes=0;                               //Number of sampled vertices
    vector<int> tmpNodes[NUM_THREADS];              //individual vectors for each thread
    int* tmpNodeSizes = new int [NUM_THREADS]();


    int k = fi*n;
    printf("%d, %lf, %d\n", n, fi, k);
    int kp;
    int VsSize = 0;
    int targetk = (int)(((float)k) * 1.1) + 2*NUM_THREADS;
    //printf("k=%ld\n",k);
    while(k>0){
    //Sample edges in parallel
        targetk = (int)(((float)k)*1.1) + 2*NUM_THREADS;
        #pragma omp parallel num_threads(NUM_THREADS)
        {
            #pragma omp for
            for(int i=0; i<NUM_THREADS; ++i) {
                tmpNodeSizes[i] = sampleEdges(edgeList, tmpNodes[i], vExist, m, n, targetk, i);
            }
        }
        kp=0;
        for(int i=0; i<NUM_THREADS; ++i)
            kp+=tmpNodeSizes[i];
        VsSize += kp;
        printf("%d, %d\n", k, kp);
        k-=kp;
    }

    kp = 0;
    //Copy values to Vs in parallel from all vectors
    #pragma omp parallel for num_threads(NUM_THREADS)
    for (int i=0; i<NUM_THREADS; i++)
    {
        int ptr = __sync_fetch_and_add(&kp, tmpNodes[i].size());
        for (int j=0; j<tmpNodes[i].size(); j++)
            Vs[ptr+j] = tmpNodes[i][j];
    }
    
    

    middle = omp_get_wtime();


    unsigned int EsSize = 0;
    int batchSize = ((VsSize-1)/NUM_THREADS)+1;
    vector<edge> tmpEdges[NUM_THREADS];
    #pragma omp parallel for num_threads(NUM_THREADS) 
    for (int i=0; i<NUM_THREADS; i++)
    {
        int start = (i*batchSize);
        int end = (i+1)*batchSize;
        end = (end < VsSize) ? end : VsSize;
        for (int j=start; j<end; j++)
        {
            int u = Vs[j];
            for (int k=g.VI[u]; k<g.VI[u+1]; k++)
            {
                int v = g.EI[k];
                if (vExist[v])
                {
                    edge e;
                    e.u = u;
                    e.v = v;
                    tmpEdges[i].push_back(e);
                } 
            }
        }
        unsigned int myCounter = __sync_fetch_and_add(&EsSize, tmpEdges[i].size()); 
        for (int j=0; j<tmpEdges[i].size(); j++)
            Es[myCounter + j] = tmpEdges[i][j];
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
