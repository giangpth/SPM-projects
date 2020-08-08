#include <miniz.h>

#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <set>

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <utility.hpp>
using namespace ff;
using namespace std;


#define BIGFILE_LOW_THRESHOLD   5000000
#define NWORKERS    16

vector<string> chunkFile(const string fullFilePath, const string chunkName, long chunkSize)
{
    cout << "Chunk file" << endl;
    vector<string> listpart;
    ifstream fileStream;
    fileStream.open(fullFilePath.c_str(), ios::in | ios::binary);
    if (fileStream.is_open())
    {
        ofstream output;
        int counter = 1;
        string fullChunkName;
        
        char* buffer = new char[chunkSize];
        while (!fileStream.eof())
        {
            fullChunkName.clear();
            fullChunkName.append(chunkName);
            // fullChunkName.append(".");

            // char intBuf[10];
            // itoa(counter, intBuf, 10);
            stringstream ss;
            ss << counter;
            
            fullChunkName.append(ss.str());

            output.open(fullChunkName.c_str(), ios::out| ios::trunc | ios::binary);
            
            if (output.is_open())
            {
                fileStream.read(buffer, chunkSize);
                output.write(buffer, fileStream.gcount());
                output.close();
                listpart.push_back(fullChunkName);

                counter++;
            }

        }
        delete[] buffer;
        fileStream.close();
        cout << "Chunking complete" << counter - 1<< " files created" << endl;
     }
     else
     {
         cout << "Error opening file" << endl;
     }
     return listpart;

}

struct Task {
    Task(size_t size, const std::string &name):
       size(size),filename(name) {}

    size_t             size;
    const std::string  filename;
};

struct Emitter:ff_node_t<Task>
{
    Emitter(const char **argv, int argc): argv(argv), argc(argc) {}

    bool doWork(const std::string& fname, size_t size) 
	{
        // cout << "Size of file is " << size << endl;
        if (size <= BIGFILE_LOW_THRESHOLD)
        {
            cout << fname << endl;
            Task* tosend = new Task(size, fname);
            ff_send_out(tosend); // sending to the next stage
            return true;
        }
        else // if big file, split and send 
        {
            // char* filename = fname.c_str();
            string chunkname = fname + ".part";
            vector<string> listpart = chunkFile(fname, chunkname.c_str(), BIGFILE_LOW_THRESHOLD);
            for (int i = 0; i < listpart.size(); i++)
            {
                struct stat statbuf;
                if (stat(listpart[i].c_str(), &statbuf)==-1) 
                {
                    perror("stat");
                    fprintf(stderr, "Error: stat %s\n", listpart[i].c_str());
                    continue;
                }
                cout << listpart[i] << " Size: " << statbuf.st_size << endl;
                Task* tosend = new Task(statbuf.st_size, listpart[i]);
                ff_send_out(tosend);
                // remove(listpart[i].c_str());
            }
            return true;
        }
    }

    bool walkDir(const std::string &dname, size_t size) 
	{
		DIR *dir;	
		if ((dir=opendir(dname.c_str())) == NULL) 
        {
			perror("opendir");
			fprintf(stderr, "Error: opendir %s\n", dname.c_str());
			return false;
		}
		struct dirent *file;
		bool error=false;
		while((errno=0, file =readdir(dir)) != NULL) 
        {
			struct stat statbuf;
			std::string filename = dname + "/" + file->d_name;
			if (stat(filename.c_str(), &statbuf)==-1) 
            {
                perror("stat");
                fprintf(stderr, "Error: stat %s\n", filename.c_str());
                return false;
			}
			if(S_ISDIR(statbuf.st_mode)) 
            {
                if ( !isdot(filename.c_str()) ) 
                {
                    if (!walkDir(filename,statbuf.st_size)) error = true;
                }
			} else 
            {
			    if (!doWork(filename, statbuf.st_size)) error = true;
			}
		}
		if (errno != 0) { perror("readdir"); error=true; }
		closedir(dir);
		return !error;
    }    
    Task* svc(Task*)
    {
        for(long i=0; i<argc; ++i) 
		{
			struct stat statbuf;
			if (stat(argv[i], &statbuf)==-1) 
            {
                perror("stat");
                fprintf(stderr, "Error: stat %s\n", argv[i]);
                continue;
			}
			bool dir=false;
			if (S_ISDIR(statbuf.st_mode)) {
			success &= walkDir(argv[i], statbuf.st_size);
			} else {
			success &= doWork(argv[i], statbuf.st_size);
			}
		}
		return EOS;
    };
    const char **argv;
    const int    argc;
    bool success = true;
};


struct Worker:ff_node_t<Task>
{
    Task* svc(Task* fname)
    { 
        //compress file
        Task &f = *fname;
        // cout << "Receive file named: " << f.filename << endl;
        unsigned char *inPtr = nullptr;
        size_t inSize = f.size;

        if (!mapFile(f.filename.c_str(), inSize, inPtr)) 
        {
            cout << "Cannot map file: " << f.filename << endl;
            return fname;
        }
        unsigned long cmp_len = compressBound(inSize);
        // allocate memory to store compressed data in memory
        unsigned char *ptrOut = new unsigned char[cmp_len];
        if (compress(ptrOut, &cmp_len, (const unsigned char *)inPtr, inSize) != Z_OK) 
        {
            printf("Failed to compress file in memory\n");	   
            success=false;
            delete [] ptrOut;
            return fname;
        }

        unmapFile(inPtr, inSize);

        //write compressed file
        string outfile = f.filename + ".zip";
        success &= writeFile(outfile, ptrOut, cmp_len);
        if (success && REMOVE_ORIGIN)
        {
            unlink(f.filename.c_str());
            
        }

        // ff_send_out(fname);
        delete[] ptrOut;
        // delete fname;
        return fname;
    }
    void svc_end()
    {
        if (!success)
        {
            cout << "Compressor stage: Exiting with (some) Error(s)" << endl;
        }
    }
    bool success = true;
};

struct Collector:ff_node_t<Task>
{
    Task* svc(Task* fname)
    {
        Task& t = *fname;
        // cout << "Collector receive " << t.filename << endl;
        int pos = t.filename.find(".part");
        if( pos != std::string::npos)
        {
            // cout << "Found sub file: " <<  t.filename << endl;
            string whole = t.filename.substr(0, pos);
            listfiles.insert(whole);
            remove(t.filename.c_str());

            listtodelete.push_back(t.filename + ".zip");
            // cout << "Whole file name: " << whole;
        }

        delete fname;
        return GO_ON;
    }
    void svc_end()
    {
        std::set<string>::iterator it;
        for(it = listfiles.begin(); it!=listfiles.end(); ++it)
        {
            // cout << *it << endl;
            stringstream ss;
            ss << "tar cf " << *it + ".zip" << " " << *it + ".part*.zip";
            // cout << ss.str() << endl; 
            system(ss.str().c_str());
        }

        for(int i = 0; i < listtodelete.size(); i++)
        {
            // cout << listtodelete[i] << endl;
            remove(listtodelete[i].c_str());
        }
        cout << "End of job" << endl; 
    }
    set<string> listfiles;
    vector<string> listtodelete;
};

void usage()
{
    cout << "./ffc_farm directory" << endl;
}



int main(int argc, char** argv)
{
    cout << argv[1] << endl;

    Emitter em(const_cast<const char**>(&argv[1]), argc);

    std::vector<std::unique_ptr<ff_node> > workers;
    for(size_t i=0;i<NWORKERS;++i) workers.push_back(make_unique<Worker>());
    
    Collector co;
    ff_Farm<Task> farm(std::move(workers), em, co);

    // ff_Farm<Task> farm(std::move(workers), em); 
    // farm.remove_collector(); 

    ffTime(START_TIME);
    if (farm.run_and_wait_end()<0) {
        error("running farm");
        return -1;
    }
    ffTime(STOP_TIME);
    std::cout << "Time: " << ffTime(GET_TIME) << "\n";
    return 0;
    // vector<string> listpart = chunkFile("/Users/phamgiang/Study/Master/SecondSemeter/SPM/TestCompress/Sisop19-1.m4v",
    // "/Users/phamgiang/Study/Master/SecondSemeter/SPM/TestCompress/Sisop19-1.m4v.part", 5242880);
    // for (int i = 0; i < listpart.size(); i++)
    // {
    //     cout << listpart[i] << endl;
    // }
}