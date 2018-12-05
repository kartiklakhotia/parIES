/**
 * Author: Kartik Lakhotia
            Aditya Gaur
 * Email id: klakhoti@usc.edu
             adityaga@usc.edu
 *
 * This code implements edge sampling
 * with induction on streaming graphs
 * 
 */

#include "head.h"

using namespace std;

#include "hash.h"

unsigned int NUM_THREADS = 36;
unsigned int BATCH = 360;
unsigned int NUM_ITER = 1;


int main(int argc, char** argv)
{

    string filename, outFilename = "";
    double f;
    bool isBinary = false;
    bool measureMem = false;
    //parse cmd line arguments//
    for (int i = 0; i < argc; i++)
    {
        if (argv[i] == string("-help") || argv[i] == string("--help") || argc == 1)
        {
            cout << "./pies -dataset/-bin **graph_file** -f **sampling ratio** -ns **number of edge cleaning synchronizations** -t **number of threads** -o **output file name** -m(measure memory consumption)" << endl;
            return 1;
        }
        if (argv[i] == string("-dataset")) 
            filename = argv[i + 1];
        if (argv[i] == string("-bin"))
        {
            filename = argv[i + 1];
            isBinary = true;
        }
        if (argv[i] == string("-f")) 
            f = atof(argv[i + 1]);
        if (argv[i] == string("-t")) 
        {
            NUM_THREADS = atoi(argv[i + 1]);
            BATCH = NUM_THREADS*100;
        }
        if (argv[i] == string("-ns"))
        {
            NUM_ITER = atoi(argv[i+1]);    
            NUM_ITER = (NUM_ITER < 1) ? 1 : NUM_ITER;
        }
        if (argv[i] == string("-o")) 
            outFilename = argv[i + 1];
        if (argv[i] == string("-m"))
            measureMem = true;
    }
    assert(f < 1);

    vector<pair<int, int>> edges;
    int V = populateEdgeList(filename, edges, isBinary);
    int Vs = (int)(f*((double)V));
    assert (Vs < V);


    /////////////////////////////////////////////////////
    //prepare edge stream to feed the algorithm//
    //If graph is disk-resident or coming from network,
    //modify here to read from disk/ethernet
    int E = edges.size();
    srand(int(time(NULL)));
    random_shuffle(edges.begin(), edges.end());
    /////////////////////////////////////////////////////


    double start = omp_get_wtime(); 
    Hash edgeSampler (Vs, NUM_THREADS); 

    int edgesProc = 0;
    int m = 0;
    printf("started sampling\n");

    //first phase - no deletions, sample till Vs is reached
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        int tid = omp_get_thread_num();
        unsigned int seed = hashInt(tid*time(NULL));
        #pragma omp for
        for (int i=0; i<NUM_THREADS; i++)
        {
            bool fin = false; //thread finish flag
            //sample all edges initially
            while(1)
            {
                int eId = __sync_fetch_and_add(&edgesProc, +NUM_THREADS);
                for (int i=0; i<NUM_THREADS; i++)
                { 
                    if (eId >= E)
                    {
                        fin = true;
                        break;
                    }
                    int u = edges[eId].first;
                    int v = edges[eId].second;
                    eId++;
                    int label_u = edgeSampler._search(u);
                    if (label_u < 0)
                    {
                        label_u = edgeSampler._insert(u, tid);
                        if (label_u < 0)
                        {
                            fin = true;
                            break;
                        }
                    }
                    int label_v = edgeSampler._search(v);
                    if (label_v < 0)
                    {
                        label_v = edgeSampler._insert(v, tid);
                        if (label_v < 0)
                        {
                            fin = true;
                            break;
                        }
                    }
                    edgeSampler.G.lockSpec(label_u);
                    edgeSampler.G.pushEdge(label_u, v);
                    edgeSampler.G.unlockSpec(label_u); 
                }
                if (fin)
                    break;
            } 
        }
    }

    m = edgesProc;
    edgesProc = edgesProc - NUM_THREADS*NUM_THREADS;
   
    int maxEdges = 0;

    int Ebatch = (E-edgesProc)/NUM_ITER + 1;
    //second phase - either induce or sample with certain probability
    while(edgesProc < E)
    {
        int targetE = Ebatch + edgesProc; 
        targetE = (targetE > E) ? E : targetE; 
        #pragma omp parallel for num_threads (NUM_THREADS)
        for (int i=0; i<NUM_THREADS; i++)
        {
            int tid = omp_get_thread_num();
            unsigned int seed = hashInt(tid*time(NULL));
            bool fin = false; //thread finish flag
            //sample new edges with some probability
            while(1)
            {
                if (edgesProc >= targetE)
                    break;
                int eId = __sync_fetch_and_add(&edgesProc, +BATCH);
                for (int en=0; en<BATCH; en++)
                {
                    if (eId >= E) //finished the stream
                    {
                        fin = true;
                        break;
                    }
                    int u = edges[eId].first;
                    int v = edges[eId].second;
                    eId++;
                    int label_u, label_v;
                    double r = (((double)rand_r(&seed)) / (RAND_MAX));
                    double p = ((double)m)/((double)eId);
                    if (r <= p)
                    {
                        label_u = edgeSampler._search(u);
                        if (label_u < 0)
                        {
                            label_u = edgeSampler._replace(u, &seed);
                            if (label_u < 0) //hash is probably full
                            {
                                fin = true;
                                break;
                            }
                        }
                        label_v = edgeSampler._search(v);
                        if (label_v < 0)
                        {
                            label_v = edgeSampler._replace(v, &seed, label_u);
                            if (label_v < 0)
                            {
                                fin = true;
                                break; 
                            }
                        } 
                    }
                    else
                    {
                        label_u = edgeSampler._search(u);
                        if (label_u < 0)
                            continue;
                        label_v = edgeSampler._search(v);
                        if (label_v < 0)
                            continue;
                    }
                    edgeSampler.G.lockSpec(label_u);
                    if ((edgeSampler.G.Vs[label_u].vId==u) && (edgeSampler.G.Vs[label_v].vId==v)){
                        edgeSampler.G.pushEdge(label_u, v);
                    }
                    edgeSampler.G.unlockSpec(label_u);
                }
                if (fin)
                    break;
            }
        }

        if (measureMem)
        {
            int ES = 0;
            #pragma omp parallel for reduction (+:ES) num_threads(NUM_THREADS)
            for (int i=0; i<Vs; i++)
                ES = ES + edgeSampler.G.Vs[i].adj.size(); 

            if (ES > maxEdges)
                maxEdges = ES;
        }

        #pragma omp parallel for num_threads(NUM_THREADS)
        for (int i=0; i<Vs; i++)
        {
            vector<int> temp;
            for (int j=0; j<edgeSampler.G.Vs[i].adj.size(); j++)
            {
                int label_v = edgeSampler._search(edgeSampler.G.Vs[i].adj[j]);
                if (label_v >= 0)
                    temp.push_back(label_v);
            }
            temp.swap(edgeSampler.G.Vs[i].adj);
        }
    }

    double stop = omp_get_wtime();
    printf("Sampling time: %lf \n", (stop-start)*1000);

    printf("sampling finished\n");
    if (measureMem)
        printf("max edges at any time = %d\n", maxEdges); 

    int ES = 0;
    #pragma omp parallel for reduction (+:ES) num_threads(NUM_THREADS)
    for (int i=0; i<Vs; i++)
        ES = ES + edgeSampler.G.Vs[i].adj.size(); 

    printf("number of edges in sampled graph = %d\n", ES);

    int numVs = 0;
    #pragma omp parallel for reduction (+:numVs) num_threads(NUM_THREADS)
    for (int i=0; i<edgeSampler.hs; i++)
       numVs += ((edgeSampler.arr[i].vId & MASK_LO)==VALID);
    
    
    //Output the sampled graph
    if(outFilename != ""){
		
		vector<pair<int, int> > outEdges;
		map<pair<int,int>,bool > m;
	
			
		for(int i=0; i<Vs; ++i){
			for(int j=0; j<edgeSampler.G.Vs[i].adj.size(); ++j){
				int v = edgeSampler.G.Vs[i].adj[j];
                int u = i;
				outEdges.push_back(pair<int, int>(u, v));
			}
		}
		
		FILE* fp = fopen(outFilename.c_str(), "w");
		if (fp==NULL)
		{
		    fputs("output file error\n", stderr);
		    return -1;
		}
		sort(outEdges.begin(),outEdges.end());
		for(int i=0; i<outEdges.size(); ++i){
			fprintf(fp,"%d %d\n",outEdges[i].first, outEdges[i].second);
		}
	}
    
    
    return 0;
}

