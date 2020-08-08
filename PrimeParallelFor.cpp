#include <iostream>
#include <vector>
#include <cmath>
#include <ff/ff.hpp>
#include <ff/parallel_for.hpp>
#include <queue>

using namespace std;
using namespace ff;
using ull = unsigned long long;

int toprint = 1;
void usage()
{
    cout << "MasterWorker begin end nw toprint";
}

static bool is_prime(ull n) 
{
    if (n <= 3)  return n > 1; // 1 is not prime !
    
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (ull i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) 
            return false;
    }
    return true;
}

int main(int argc, char *argv[]) {    
    if (argc<5) {
        usage();
        return -1;
    }
    ull begin = atoi(argv[1]);
    ull end = atoi(argv[2]);
    int nw = atoi(argv[3]);
    if (atoi(argv[4]) == 0)
    {
        toprint = 0;
    }
    

    cout <<"Range: " <<  begin << " " << end << endl;
    
    // std::priority_queue<int, std::vector<int>, std::greater<int> > result;;
    vector<ull> result;
    result.reserve((size_t)(end-begin)/log(begin));

    ffTime(START_TIME);
    if (nw>1) {
        // 2*nworkers is just to have a different number
        // if no parameter is passed, it will be started ff_numCores() threads
        ParallelFor pf(2*nw);

        cout << "Create pf done" << endl;

        pf.parallel_for(begin,end, 1, 
                        [&](const long i) 
                        { 
                            if(is_prime(i))
                            {
                                result.push_back(i);
                            }
                        }, nw);

    }  
    ffTime(STOP_TIME);  
    
    if (toprint == 1)
    {
        sort(result.begin(), result.end());
        cout << "Final result " << endl;
        for (int i = 0; i < result.size(); i++)
        {
            cout << result[i] << " ";
        }
        cout << endl;
    }
    
    std::cout << "Time: " << ffTime(GET_TIME) << "\n";
    return 0;
}
