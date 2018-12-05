typedef struct Graph
{
    unsigned int numVertex;
    unsigned int numEdges;
    unsigned int* VI;
    unsigned int* EI;
} Graph;

typedef struct edge
{
    int u, v;
} edge;

int read_csr (const char* filename, Graph* G)
{
    FILE* graphFile = fopen(filename, "rb");
    if (graphFile == NULL)
    {
        fputs("file error", stderr);
        return -1;
    }
    fread (&(G->numVertex), sizeof(unsigned int), 1, graphFile);
    fread (&(G->numEdges), sizeof(unsigned int), 1, graphFile);


    G->VI = new unsigned int[G->numVertex];

    fread (G->VI, sizeof(unsigned int), G->numVertex, graphFile);
    if (feof(graphFile))
    {
        delete[] G->VI;
        printf("unexpected end of file while reading vertices\n");
        return -1;
    }
    else if (ferror(graphFile))
    {
        delete[] G->VI;
        printf("error reading file\n");
        return -1;
    }

    G->EI = new unsigned int[G->numEdges];
    fread (G->EI, sizeof(unsigned int), G->numEdges, graphFile);
    if (feof(graphFile))
    {
        delete[] G->VI;
        delete[] G->EI;
        printf("unexpected end of file while reading edges\n");
        return -1;
    }
    else if (ferror(graphFile))
    {
        delete[] G->VI;
        delete[] G->EI;
        printf("error reading file\n");
        return -1;
    }

    fclose(graphFile);
    return 1;
}

void writeGraph(string filename,edge *Es, int EsSize){
    ofstream out(filename,ofstream::out);
    int i;
    for(i=0; i<EsSize; ++i){
        if ((Es[i].u > 0) && (Es[i].v > 0))
            out<<Es[i].u<<" "<<Es[i].v<<'\n';
    }
    out.close();
}


int inputGraph(string filename, vector<edge> &el, Graph *G){ //creates the edge list in the passed vector, and returns the number of nodes(maximum node found in any edge)

    ifstream in(filename, ifstream::in);
    int a,b; 		//input nodes for the edge
	read_csr(filename.c_str(), G);

    for(int i=0; i<G->numVertex; ++i){
		int start= G->VI[i];
		int end = G->VI[i+1];
		for(int j=start; j<end; ++j){
        	int vertex = G->EI[j];
            edge e;
            e.u = i;
            e.v = vertex;
        	el.push_back(e);
        }
    }
    dbg(printf("%ld\n",G->numEdges);)
    return G->numVertex;
}

