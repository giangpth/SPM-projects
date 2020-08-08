#include <iostream>
#include <vector>
#include <math.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <cstring>
#include <omp.h>

using namespace std;
using namespace std::chrono;

#define START(timename) auto timename = std::chrono::system_clock::now();
#define STOP(timename,elapsed)  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - timename).count();


class utimer {
  std::chrono::system_clock::time_point start;
  std::chrono::system_clock::time_point stop;
  std::string message; 
  using usecs = std::chrono::microseconds;
  using msecs = std::chrono::milliseconds;

private:
  long * us_elapsed;
  
public:

  utimer(const std::string m) : message(m),us_elapsed((long *)NULL) {
    start = std::chrono::system_clock::now();
  }
    
  utimer(const std::string m, long * us) : message(m),us_elapsed(us) {
    start = std::chrono::system_clock::now();
  }

  ~utimer() {
    stop =
      std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed =
      stop - start;
    auto musec =
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    
    std::cout << message << " computed in \t \t" << musec << " usec " 
	      << std::endl;
    if(us_elapsed != NULL)
      (*us_elapsed) = musec;
  }
};


typedef int aint __attribute__ ((__aligned__(16)));

void genMap(int* map, int nrow, int ncol, int seed)
{
    utimer t ("Gen map");
    srand(seed);
    for (int i = 0; i < nrow ; i++) 
    {
        for (int j = 0; j < ncol ; j++) 
        {
            map[i*ncol + j] = rand()%2;
            if(j == 0 || j == ncol - 1 || i == 0 || i == nrow -1) 
            {
                map[i*ncol+j] = 0;    
            }   
        }
    }
    
}

void showMap (int* map, int nrow, int ncol)
{
    utimer t("Show map");
    for (int i = 0; i < nrow; i++)
    {
        for(int j = 0; j < ncol; j++)
        {
            if (map[i*ncol + j])
            {
                std::cout << "0";
            }
            else
            {
                std::cout << "_";
            }
        }
        std::cout << endl;
    }
    std::cout << endl;
}

void simpleSeq(int* map, int nrow, int ncol)
{
    utimer t("Simple sequential");
    int* sum = new int[nrow*ncol];
    {
        // utimer f1("Simple sequential for 1");
        for (int i = 1; i < nrow - 1 ; i++)
        {
            for (int j = 1; j < ncol - 1; j++)
            {
                sum[i*ncol + j] = map[(i-1)*ncol + (j-1)] + map[(i-1)*ncol + (j)] + map[(i-1)*ncol + (j+1)]
                                + map[i*ncol + (j - 1)] + map[i*ncol + (j+1)]
                                + map[(i+1)*ncol + (j-1)] + map[(i+1)*ncol + (j)] + map[(i+1)*ncol + (j+1)];
            }
        }
    }
    {
        // utimer f2("Simple sequential for 2");
        for (int i = 1; i < nrow - 1; i++)
        {
            for (int j = 1; j < ncol - 1; j++)
            {
                if (sum[i*ncol + j] == 3)
                {
                    map[i*ncol + j] = 1;
                }
                else if (sum[i*ncol + j] == 2)
                {
                    map[i*ncol + j] = map[i*ncol + j];
                }
                else
                {
                    map[i*ncol + j] = 0;
                }
                
            }
        }
    }
    delete [] sum;
}

bool compareRes(int* res1, int* res2, int size)
{
    for(int i = 0; i < size; i++)
    {
        if (res1[i] != res2[i])
        {
            return false;
        }
    }
    return true;
}

int* golomp(int* map, int nrow, int ncol, int nw)
{
    utimer t("Omp version ");
    int onepartrow = ((nrow - 2)/nw); // only calculated row
    int lastpartrow = ((nrow - 2) - onepartrow*(nw-1)); // only calculated row
    aint* __restrict__ res = new int[nrow*ncol];
    for (int i = 0; i < ncol + 1; i++)
    {
        res[i] = 0;
        res[nrow*ncol - 1 - i] = 0;
    }
    auto calChunk = [&] (int idx)
    {
        // utimer t("Frome thread " + to_string(idx));
        int thispartrow; //number of row + 1 upper row + 1 lower row 
        idx != nw-1? thispartrow = (onepartrow + 2) : thispartrow = (lastpartrow + 2);
        int thispart = thispartrow*ncol;
        int first = idx*onepartrow*ncol;
        int offset = ncol + 1;

        int* sum = new int[thispart];
        sum[0] = 0; sum[thispart-1] = 0;
        {
            // utimer f1("Thread version for 1");
            for (int i = offset; i < thispart - offset; i++)
            {
                int mapidx = i + first;
                sum[i] = map[mapidx-offset] + map[mapidx-offset+1] + map[mapidx-offset+2] +
                        map[mapidx - 1] + map[mapidx+1] +
                        map[mapidx + offset - 2] + map[mapidx + offset - 1] + map[mapidx + offset];

            }
        }
        {
            // utimer f1("Thread version for 2");
            for(int i = 1; i < thispartrow - 1; ++i)
            {  
                res[i*ncol] = 0;
                res[i*ncol + ncol - 1] = 0;
                for(int j = 1; j < ncol - 1; ++j)
                {
                    if (sum[i*ncol + j] == 3)
                    {
                        res[first + i*ncol + j] = 1;
                    }
                    else if(sum[i*ncol + j] == 2)
                    {
                        res[first + i*ncol + j] = map[first+ i*ncol + j];
                    }
                    else
                    {
                        res[first + i*ncol + j] = 0;
                    }
                    
                }
            }
        }
        delete [] sum;    
    };
#pragma omp parallel num_threads(nw)
    {
        auto id = omp_get_thread_num();
        calChunk(id);
    }
    delete [] map;
    return res;
}

int* golthread(int* map, int nrow, int ncol, int nw)
{
    utimer t("Thread version ");
    int onepartrow = ((nrow - 2)/nw); // only calculated row
    int lastpartrow = ((nrow - 2) - onepartrow*(nw-1)); // only calculated row
    aint* __restrict__ res = new int[nrow*ncol];
    for (int i = 0; i < ncol + 1; i++)
    {
        res[i] = 0;
        res[nrow*ncol - 1 - i] = 0;
    }
    auto calChunk = [&] (int idx)
    {
        // utimer t("Frome thread " + to_string(idx));
        int thispartrow; //number of row + 1 upper row + 1 lower row 
        idx != nw-1? thispartrow = (onepartrow + 2) : thispartrow = (lastpartrow + 2);
        int thispart = thispartrow*ncol;
        int first = idx*onepartrow*ncol;
        int offset = ncol + 1;

        int* sum = new int[thispart];
        sum[0] = 0; sum[thispart-1] = 0;
        {
            // utimer f1("Thread version for 1");
            for (int i = offset; i < thispart - offset; i++)
            {
                int mapidx = i + first;
                sum[i] = map[mapidx-offset] + map[mapidx-offset+1] + map[mapidx-offset+2] +
                        map[mapidx - 1] + map[mapidx+1] +
                        map[mapidx + offset - 2] + map[mapidx + offset - 1] + map[mapidx + offset];

            }
        }
        {
            // utimer f1("Thread version for 2");
            for(int i = 1; i < thispartrow - 1; ++i)
            {  
                res[i*ncol] = 0;
                res[i*ncol + ncol - 1] = 0;
                for(int j = 1; j < ncol - 1; ++j)
                {
                    if (sum[i*ncol + j] == 3)
                    {
                        res[first + i*ncol + j] = 1;
                    }
                    else if(sum[i*ncol + j] == 2)
                    {
                        res[first + i*ncol + j] = map[first+ i*ncol + j];
                    }
                    else
                    {
                        res[first + i*ncol + j] = 0;
                    }
                    
                }
            }
        }
        delete [] sum;    
    };
    vector<thread> tids;
    for(int i = 0; i < nw; i++)
    {
        tids.push_back(thread(calChunk,i));
    }
    for(thread& t:tids)
    {
        t.join();
    }
    delete [] map;
    return res;
}

void usage()
{
    std::cout << "Number of rows, number of cols, random seed, parallelism degree, number of iterations" << endl;
}

int main(int argc, char ** argv)
{
    if (argc < 6)
    {
        usage();
        return 0;
    }
    else
    {
        int nrow    = atoi(argv[1]);
        int ncol    = atoi(argv[2]);
        int seed    = atoi(argv[3]);
        int nw      = atoi(argv[4]);
        int iter    = atoi(argv[5]);

        aint* __restrict__ map1 = new int[nrow*ncol]; 
        genMap(map1, nrow, ncol, seed);
       
        aint* __restrict__ map2 = new int[nrow*ncol];
        memcpy(map2, map1, sizeof(int)*nrow*ncol);

        aint* __restrict__ map3 = new int[nrow*ncol];
        memcpy(map3, map1, sizeof(int)*nrow*ncol);


        
        for (int i = 0; i < iter; i++)
        {
            simpleSeq(map1, nrow, ncol);
            // showMap(map1, nrow, ncol);
        }

        for (int i = 0; i < iter; i++)
        {
            map2 = golthread(map2, nrow, ncol, nw);
        }

        for (int i = 0; i < iter; i++)
        {
            map3 = golomp(map3, nrow, ncol, nw);
        }

        if(compareRes(map1, map2, nrow*ncol))
        {
            cout << "Thread version is correct" << endl;
        }
        else
        {
            cout <<"Thread version is incorrect" << endl;
        }

        if(compareRes(map1, map3, nrow*ncol))
        {
            cout << "Omp version is correct" << endl;
        }
        else
        {
            cout <<"Omp version is incorrect" << endl;
        }
            
        delete [] map1;
        delete [] map2;
        delete [] map3;

    }
    return 0;
}