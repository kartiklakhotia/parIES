inline int hashInt(int a) {
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return ((a < 0) ? -a : a);
}

int populateEdgeList(string filename, vector<pair<int, int>> &edges, bool isBinary)
{

    int V = 0;
    int E = 0;
    int src, dst;

    if (!isBinary)
    {
        FILE* fp = fopen(filename.c_str(), "r");
        if (fp==NULL)
        {
            fputs("file error", stderr);
            return -1;
        }
        while(!feof(fp))
        {
            if (fscanf(fp, "%d", &src) <= 0)
                break;
            if (fscanf(fp, "%d", &dst) <= 0)
                break;
            V = (src > V) ? src : V;
            V = (dst > V) ? dst : V;
            E++;
            edges.push_back(make_pair(src, dst));
        }
        V++;
        fclose(fp);
    }
    else
    {
        FILE* fp = fopen(filename.c_str(), "rb");
        if (fp==NULL)
        {
            fputs("file error", stderr);
            return -1;
        }

        fread (&V, sizeof(int), 1, fp);
        fread (&E, sizeof(int), 1, fp); 

        int* VI = new int [V+1];
        fread(VI, sizeof(int), V, fp);
        if (feof(fp))
        {
            delete[] VI;
            printf("unexpected end of file while reading vertices\n");
            return -1;
        }
        VI[V] = E;

        int* EI = new int [E];
        fread(EI, sizeof(int), E, fp);
        if (feof(fp))
        {
            delete[] EI;
            delete[] VI;
            printf("unexpected end of file while reading edges\n");
            return -1;
        }

        for (int i=0; i<V; i++)
            for (int j=VI[i]; j<VI[i+1]; j++)
               edges.push_back(make_pair(i, EI[j])); 

        delete[] VI;
        delete[] EI;
    }

    return V;
}

