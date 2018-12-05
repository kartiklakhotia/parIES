
#define REALLOC_THRESH 10

int GROUP = 36;

typedef struct sampleV_item{
    bool lock;
    int vId; // #original id of the vertex relabelled to this location
    vector<int> adj; 
} sampleV_item;

class sampleV{
private:
    int ss; // #of vertices to be sampled
    int numSamples; // # of sampled vertices
    int* reserve;
public:
    sampleV_item* Vs;
    sampleV(int size, unsigned int NT)
    {
        Vs = new sampleV_item[size];
        reserve = new int [NT*64]();
        GROUP = NT;
        ss = size;
        numSamples = 0;
        for (int i=0; i<ss; i++)
            Vs[i].lock = false;
    } 

    int insertInVs(int vId, int tId)
    {
        int loc = reserve[tId << 6];
        if ((loc % GROUP) == 0)
            loc = __sync_fetch_and_add(&numSamples, +GROUP);
        if (loc >= ss) 
            return -1;
        reserve[tId << 6] = loc + 1;
        Vs[loc].vId = vId;
        return loc;
    }

    int lockRandom(unsigned int* seed)
    {
        int loc = (rand_r(seed) % ss); 
        while(!__sync_bool_compare_and_swap(&(Vs[loc].lock), false, true))
            loc = (rand_r(seed) % ss);
        return loc;
    }

    void lockSpec(int loc)
    {
        while(!__sync_bool_compare_and_swap(&(Vs[loc].lock), false, true)){;}
    }

    void unlockSpec(int loc)
    {
        Vs[loc].lock = false;
    }

    int replaceInVs(int vId, int loc)
    {
        int temp = Vs[loc].vId;
        Vs[loc].vId = vId;
        Vs[loc].adj.clear();
        if (Vs[loc].adj.capacity() > REALLOC_THRESH) 
            Vs[loc].adj.shrink_to_fit();
        return temp;
    }

    void pushEdge(int loc, int v)
    {
        Vs[loc].adj.push_back(v);
    }

    ~sampleV()
    {
        delete[] Vs;
    }
};
