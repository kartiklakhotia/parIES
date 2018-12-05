#include "utils.h"
#include "sampleV.h"


#define EMPTY 0x00000000
#define VALID 0x40000000
#define BUSY 0x80000000
#define MASK_HI 0x3FFFFFFF
#define MASK_LO 0xC0000000

#define HASH_SIZE_RATIO 2


typedef struct Hash_item{
    bool lock;
    int count; // # of entries hashed before this item but placed afterwards
    int vId; // # original id of the vertex being placed
    int index; // # new label of the vertex in the dynamic output graph
} Hash_item;

class Hash{
private:
public:
    int hs;
    sampleV G;
    Hash_item* arr;
    Hash(int size, unsigned int NT) : G(size, NT){
        hs = HASH_SIZE_RATIO*size;
        arr = new Hash_item[hs];
        for (int i=0; i<hs; i++)
        {
            arr[i].vId = EMPTY;
            arr[i].lock = false; 
            arr[i].count = 0;
        }
        
    }

    int _search(int val){
        int h = (hashInt(val)) % hs;
        int loc = h;
        int validVal = (val | VALID);
        while(1)
        {
            if (arr[loc].vId == validVal)
                return arr[loc].index;

            if (arr[loc].count <= 0)
                break;
            //increment h
            loc = (loc + 1) % hs;
            //failsafe. quit if completed one full scan of the hash
            if (loc == h)
                break;
        }
        return -1;
    }

    //overloaded function. If the search should begin from a specified location
    int _search(int val, int h){
        int loc = h;
        int validVal = (val | VALID);
        while(1)
        {
            if (arr[loc].vId == validVal)
                return arr[loc].index;

            if (arr[loc].count <= 0)
                break;
            //increment h
            loc = (loc + 1) % hs;

            //failsafe. quit if completed one full scan of the hash
            if (loc == h)
                return -1;
        }
        return -1;
    }

    //initial insertion - no deletion at this point
    int _insert(int val, int tId){
        int h = (hashInt(val)) % hs;
        int loc = h;
        int validVal = (val | VALID);
        int busyVal = (val | BUSY);

        Hash_item &temp_h = arr[h];
        while(!__sync_bool_compare_and_swap(&(temp_h.lock), false, true)){}
        while(1)
        {
            Hash_item &temp = arr[loc];
            //element is already present and found before an empty location
            if (temp.vId == validVal)
            {
                temp_h.lock = false;
                return temp.index; 
            }
            //found an empty location
            else if (__sync_bool_compare_and_swap(&(temp.vId), (temp.vId & MASK_HI) | EMPTY, busyVal)) 
            {
                //temp.vId = val;
                temp.index = G.insertInVs(temp.vId & MASK_HI, tId);
                //if Vs was already full//
                if (temp.index < 0)
                {
                    temp.vId = EMPTY; //(temp.vId & MASK_HI) | EMPTY;
                }
                else
                {
                    int rstLoc = h;
                    while(1)
                    {
                        if (rstLoc == loc)
                            break;  
                        __sync_fetch_and_add(&arr[rstLoc].count, +1);
                        rstLoc = (rstLoc + 1) % hs;
                    }
                    temp.vId = (temp.vId & MASK_HI) | VALID;
                }
                temp_h.lock = false;
                return temp.index; 
            }
            loc = (loc + 1) % hs;
//            failsafe. quit if completed one full scan of the hash
            if (loc == h)
                break;
        }
        temp_h.lock = false; 
        return -1;
    }

    int _replace(int val, unsigned int* seed, int u=-1){
        int h = (hashInt(val)) % hs;
        int loc = h;

        int validVal = (val | VALID);
        int busyVal = (val | BUSY);

        int retVal;
        Hash_item &temp_h = arr[h];
        while(!__sync_bool_compare_and_swap(&(temp_h.lock), false, true)){}
        while(1)
        {
            Hash_item &temp = arr[loc];
            //element is already present and found before an empty location
//            if (temp.flag==VALID && temp.vId==val)
            if (temp.vId == validVal)
            {
                retVal = temp.index;
                temp_h.lock = false;
                return retVal; 
            }
            //found an empty location
            else if (__sync_bool_compare_and_swap(&(temp.vId), (temp.vId & MASK_HI) | EMPTY, busyVal)) 
            {
                //temp.vId = val;
                //search further ahead for a copy of val
                int existLoc = _search(val, loc);
                //no copy of val exists
                if (existLoc < 0)
                {
                    temp.index = G.lockRandom(seed);
                    while((temp.index == u)){
                    	G.unlockSpec(temp.index);
                    	temp.index = G.lockRandom(seed);
                    }
                    int prevId = G.replaceInVs(temp.vId & MASK_HI, temp.index);
                    _delete(prevId);
                    int rstLoc = h;
                    while(1)
                    {
                        if (rstLoc == loc)
                            break;  
                        __sync_fetch_and_add(&arr[rstLoc].count, +1);
                        rstLoc = (rstLoc + 1) % hs;
                    }
                    temp.vId = (temp.vId & MASK_HI) | VALID;
                    retVal = temp.index;
                    temp_h.lock = false;
                    G.unlockSpec(temp.index);
                    return retVal; 
                }    
                //copy of val exists after the first empty location
                else
                {
                    temp.vId = EMPTY; //(temp.vId & MASK_HI) | EMPTY;
                    temp_h.lock = false;
                    return existLoc;
                }
            }
            loc = (loc + 1) % hs;
//          failsafe. quit if completed one full scan of the hash
            if (loc == h)
                break;
        }
        temp_h.lock = false; 
        return -1;
    }

    int _delete(int val){
        int h = (hashInt(val)) % hs;
        int loc = h;

        int validVal = (val | VALID);

        Hash_item &temp_h = arr[h];
        while(1)
        {   
            Hash_item &temp = arr[loc];
//            if ((arr[loc].flag == VALID) && (arr[loc].vId==val))
            if (__sync_bool_compare_and_swap(&(temp.vId), validVal, EMPTY)) 
            {
                //arr[loc].flag = EMPTY;
                int rstLoc = h;
                while (1)
                {
                    if (rstLoc==loc)
                        break;
                    __sync_fetch_and_add(&arr[rstLoc].count, -1);
                    rstLoc = (rstLoc + 1) % hs;
                }
                return val;
            }
            if (arr[loc].count <= 0)
                break;
            loc = (loc + 1) % hs;
//          failsafe. quit if completed one full scan of the hash
            if (loc==h)
                break;
        }
        //not found
        return -1;
    }

    ~Hash()
    {
        delete[] arr;
    }
};


