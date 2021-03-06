#include "rose.h"
#include "compilationFileDatabase.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>
#include <iostream>
#include <sstream>
#include <functional>
#include <numeric>
#include <fstream>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "common.h"

using namespace std;
using namespace boost::algorithm;
using namespace SageBuilder;
using namespace SageInterface;

static FILE * __roseTraceFilePtr;
static char * __roseTraceFile;
static FILE * __roseProjectDBFilePtr;
static const char * __roseProjectDBFile;
static std::vector<string> idToFile;

static boost::unordered_map<uint64_t, SgNode*> idToNodeMap;

// A simple preorder traversal to assign unique ids to each node.
// The same order is used in in NodeToIdMapper for instrumentation.
class IdToNodeMapper: public AstSimpleProcessing {
    string m_fileUnderCompilation;
    uint32_t m_fileIdUnderCompilation;
    uint32_t m_counter;
    string m_roseDBFile;
    
    
public:
    IdToNodeMapper(SgFile * fileRoot, const string & dbFile){
        m_fileUnderCompilation = fileRoot->get_file_info()->get_physical_filename();
        m_roseDBFile = dbFile;
        m_counter = 0;
        m_fileIdUnderCompilation  = rose::GetProjectWideUniqueIdForPhysicalFile(m_roseDBFile, m_fileUnderCompilation);
        traverse(fileRoot, preorder);
    }
    
    
private:
    void virtual visit(SgNode * node) {
        TraceId id(m_fileIdUnderCompilation, m_counter++);
        idToNodeMap[id.GetSerializableId()] = node;
        cout<<"\n"<< std::hex << id.GetSerializableId() << " : mapped to " << std::hex << node;
        
    }
    
};


int main( int argc, char * argv[] ) {
    
    if(argc < 2){
        fprintf(stderr, "\n Usage %s <TraceFile> rest of the arguments to rose .. must pass -rose:projectSpecificDatabaseFile", argv[0]);
        exit(-1);
    }
    
    __roseTraceFile = argv[1];
    
    if(!(__roseTraceFilePtr = fopen(__roseTraceFile, "rb"))){
        fprintf(stderr, "\n Failed to read TraceFile %s",__roseTraceFile);
        exit(-1);
    }
    
    
    // patch argv
    for(int i = 2 ; i < argc; i++){
        argv[i-1] = argv[i];
    }
    
    
    
    // Generate the ROSE AST.
    SgProject* project = frontend(argc-1,argv);
    // AST consistency tests (optional for users, but this enforces more of our tests)
    AstTests::runAllTests(project);
    
    
    __roseProjectDBFile = project->get_projectSpecificDatabaseFile().c_str();
    if(!(__roseProjectDBFilePtr = fopen(__roseProjectDBFile, "r"))){
        fprintf(stderr, "\n Failed to read roseDBFile %s",__roseProjectDBFile);
        exit(-1);
    }
    
    char fileName[PATH_MAX];
    // Read file DB
    uint32_t fileId = 0;
    while(fgets(fileName, PATH_MAX, __roseProjectDBFilePtr)){
        // kill \n
        fileName[strlen(fileName)-1] = '\0';
        idToFile.push_back(string(fileName));
    }
    fclose(__roseProjectDBFilePtr);
    
    // Build node to trace id map.
    for(int i = 0 ; i < project->numberOfFiles(); i++) {
        SgFile & file = project->get_file(i);
        IdToNodeMapper mapper(&file, project->get_projectSpecificDatabaseFile());
    }
    
    // Read and print all trace records
    uint64_t traceId;
    std::cout<<"\n TraceId : File : SgNode * : class_name()";
    cout<<"\n *******************************";
    
    boost::unordered_map<uint64_t, SgNode*>::iterator it;
    
    while(fread(&traceId, sizeof(uint64_t), 1, __roseTraceFilePtr)){
        uint32_t fileId = traceId >> 32;
        uint32_t nodeId = (traceId & 0xffffffff);
        
        if (fileId > idToFile.size()) {
            cout<<"\n"<< std::hex << traceId << " MISSING FILE!!!! id" << std::hex << fileId;
        } else {
            it = idToNodeMap.find(traceId);
            if(it == idToNodeMap.end()){
                cout<<"\n"<< std::hex << traceId << " can't map back!!!" << std::hex << fileId << std::hex << nodeId;
            } else {
                cout<<"\n"<< std::hex << traceId << ":"<< idToFile[fileId] << ":" << std::hex << (*it).second << ":" << ((*it).second)->class_name();
            }
        }
    }
    cout<<"\n *******************************";
    fclose(__roseTraceFilePtr);
    
    return 0;
    
    
    
    
}
