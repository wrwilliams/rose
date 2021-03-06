// Example ROSE Translator used for testing ROSE infrastructure
#include "rose.h"

// DQ (3/5/2017): Adding support for EDG/ROSE frontend message logging.
#ifndef ROSE_USE_CLANG_FRONTEND
// DQ (2/5/2017): This is only used with the EDG frontend, not for use when configured to use Clang.
namespace EDG_ROSE_Translation
   {
     extern Sawyer::Message::Facility mlog;
   }
#endif

int main( int argc, char * argv[] )
   {
#if 0
  // Output the ROSE specific predefined macros.
     outputPredefinedMacros();
#endif

  // DQ (3/5/2017): This will not fail if we skip calling ROSE_INITIALIZE (but the test for rose::Diagnostics::isInitialized() 
  // is then required, however it will fail to output message that we enable explicitly below.
  // Initialize and check compatibility. See rose::initialize
     ROSE_INITIALIZE;

  // DQ (3/5/2017): Add message logging by to be on by default for testing (disable conditional support for testing).
#if defined(ROSE_BUILD_CXX_LANGUAGE_SUPPORT) && !defined(ROSE_USE_CLANG_FRONTEND)
  // Note that we have to first check if initialization has been called.
  // printf ("rose::Diagnostics::isInitialized() = %s \n",rose::Diagnostics::isInitialized() ? "true" : "false");
     if (rose::Diagnostics::isInitialized() == true) 
        {
       // Command line options are available to control message streams.
       // To get help use: -rose:log help
       // Note that alternative mesage streams can be turned on from the command line:
       //    to get list of streams: -rose:log list
       //    to activate specific messge streams use (e.g.): -rose:log none,EDG_ROSE_Translation::(debug)
       //    there are numerous additional options...
#if 0
       // DQ (3/5/2017): Disable to support evaluation of ROSE compilationwithout output spew 
       // (then convert those messge to use the message log).

       // DQ (3/5/2017): Allow output of diagnostic messages from the EDG/ROSE translation.
          EDG_ROSE_Translation::mlog[rose::Diagnostics::DEBUG].enable(true);

       // DQ (3/5/2017): Allow output of diagnostic messages from the ROSE IR nodes.
          rose::ir_node_mlog[rose::Diagnostics::DEBUG].enable(true);
#endif

#if 1
       // DQ (3/6/2017): Test API to set frontend and backend options for tools (minimal output from ROSE-based tools).
       // Note that the defaults are for minimal output from ROSE-based tools.
          rose::global_options.set_frontend_notes(false);
          rose::global_options.set_frontend_warnings(false);
          rose::global_options.set_backend_warnings(false);
#endif
        }
#endif

  // Generate the ROSE AST (note ROSE_INITIALIZE will be called by the frontend, if not called explicitly above in main()).
     SgProject* project = frontend(argc,argv);

  // AST consistency tests (optional for users, but this enforces more of our tests)
     AstTests::runAllTests(project);

  // DQ (3/20/2017): Test info about mode (code coverage).
     ROSE_ASSERT(SageBuilder::display(SageBuilder::SourcePositionClassificationMode) == "e_sourcePositionTransformation");

  // DQ (3/20/2017): Test this function after EDG/ROSE translation (not required for users).
     SageBuilder::clearScopeStack();

#if 0
  // Output an optional graph of the AST (just the tree, when active)
     printf ("Generating a dot file... (ROSE Release Note: turn off output of dot files before committing code) \n");
     generateDOT ( *project );
     // generateAstGraph(project, 2000);
#endif

#if 0
  // Output an optional graph of the AST (the whole graph, of bounded complexity, when active)
     const int MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH = 8000;
     generateAstGraph(project,MAX_NUMBER_OF_IR_NODES_TO_GRAPH_FOR_WHOLE_GRAPH);
#endif

#if 0
     SgNode::get_globalTypeTable()->print_typetable();
#endif

  // regenerate the source code and call the vendor 
  // compiler, only backend error code is reported.
     return backend(project);
   }
