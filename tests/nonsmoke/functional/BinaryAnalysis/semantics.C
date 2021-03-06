/* For each function (SgAsmFunction) process each instruction (SgAsmInstruction) through the instruction semantics
 * layer using the FindConstantsPolicy. Output consists of each instruction followed by the registers and memory locations
 * with constant or pseudo-constant values. */
#include "conditionalDisable.h"
#ifdef ROSE_BINARY_TEST_DISABLED
#include <iostream>
int main() { std::cout <<"disabled for " <<ROSE_BINARY_TEST_DISABLED <<"\n"; return 1; }
#else

#define __STDC_FORMAT_MACROS
#include "rose.h"
#include <set>
#include <inttypes.h>
#include <Sawyer/IntervalSet.h>

// SEMANTIC_DOMAIN values
#define NULL_DOMAIN 1
#define PARTSYM_DOMAIN 2
#define SYMBOLIC_DOMAIN 3
#define INTERVAL_DOMAIN 4
#define MULTI_DOMAIN 5

// SMT_SOLVER values
#define NO_SOLVER 0
#define YICES_LIB 1
#define YICES_EXE 2

#include "DispatcherX86.h"
#include "TestSemantics2.h"
using namespace rose::BinaryAnalysis::InstructionSemantics2;

#if !defined(SMT_SOLVER) || SMT_SOLVER == NO_SOLVER
#   include "SMTSolver.h"
    rose::BinaryAnalysis::SMTSolver *make_solver() { return NULL; }
#elif SMT_SOLVER == YICES_LIB
#   include "YicesSolver.h"
    rose::BinaryAnalysis::SMTSolver *make_solver() {
        rose::BinaryAnalysis::YicesSolver *solver = new rose::BinaryAnalysis::YicesSolver;
        solver->set_linkage(rose::BinaryAnalysis::YicesSolver::LM_LIBRARY);
        return solver;
    }
#elif SMT_SOLVER == YICES_EXE
#   include "YicesSolver.h"
    rose::BinaryAnalysis::SMTSolver *make_solver() {
        rose::BinaryAnalysis::YicesSolver *solver = new rose::BinaryAnalysis::YicesSolver;
        solver->set_linkage(rose::BinaryAnalysis::YicesSolver::LM_EXECUTABLE);
        return solver;
    }
#else
#   error "invalid value for SMT_SOLVER"
#endif

const RegisterDictionary *regdict = RegisterDictionary::dictionary_pentium4();

#include "TraceSemantics2.h"

// defaults for command-line switches
static bool do_trace = false;
static bool do_test_subst = false;
static bool do_usedef = true;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if SEMANTIC_DOMAIN == NULL_DOMAIN

#   include "NullSemantics2.h"
    static BaseSemantics::RiscOperatorsPtr make_ops() {
        BaseSemantics::RiscOperatorsPtr retval = NullSemantics::RiscOperators::instance(regdict);
        TestSemantics<
            NullSemantics::SValuePtr, NullSemantics::RegisterStatePtr, NullSemantics::MemoryStatePtr,
            BaseSemantics::StatePtr, NullSemantics::RiscOperatorsPtr> tester;
        tester.test(retval);
        return retval;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#elif  SEMANTIC_DOMAIN == PARTSYM_DOMAIN
#   include "PartialSymbolicSemantics2.h"
    static BaseSemantics::RiscOperatorsPtr make_ops() {
        BaseSemantics::RiscOperatorsPtr retval = PartialSymbolicSemantics::RiscOperators::instance(regdict);
        TestSemantics<PartialSymbolicSemantics::SValuePtr, BaseSemantics::RegisterStateGenericPtr,
                      BaseSemantics::MemoryCellListPtr, BaseSemantics::StatePtr,
                      PartialSymbolicSemantics::RiscOperatorsPtr> tester;
        tester.test(retval);
        return retval;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#elif  SEMANTIC_DOMAIN == SYMBOLIC_DOMAIN

#   include "SymbolicSemantics2.h"
    static BaseSemantics::RiscOperatorsPtr make_ops() {
        SymbolicSemantics::RiscOperatorsPtr retval = SymbolicSemantics::RiscOperators::instance(regdict);
        retval->computingDefiners(do_usedef ? SymbolicSemantics::TRACK_ALL_DEFINERS : SymbolicSemantics::TRACK_NO_DEFINERS);
        TestSemantics<SymbolicSemantics::SValuePtr, BaseSemantics::RegisterStateGenericPtr,
                      SymbolicSemantics::MemoryStatePtr, BaseSemantics::StatePtr,
                      SymbolicSemantics::RiscOperatorsPtr> tester;
        tester.test(retval);
        return retval;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#elif SEMANTIC_DOMAIN == INTERVAL_DOMAIN
#   include "IntervalSemantics2.h"
    static BaseSemantics::RiscOperatorsPtr make_ops() {
        BaseSemantics::RiscOperatorsPtr retval = IntervalSemantics::RiscOperators::instance(regdict);
        TestSemantics<IntervalSemantics::SValuePtr, BaseSemantics::RegisterStateGenericPtr,
                      IntervalSemantics::MemoryStatePtr, BaseSemantics::StatePtr,
                      IntervalSemantics::RiscOperatorsPtr> tester;
        tester.test(retval);
        return retval;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#elif SEMANTIC_DOMAIN == MULTI_DOMAIN
#   include "MultiSemantics2.h"
#   include "PartialSymbolicSemantics2.h"
#   include "SymbolicSemantics2.h"
#   include "IntervalSemantics2.h"
    static BaseSemantics::RiscOperatorsPtr make_ops() {
        MultiSemantics::RiscOperatorsPtr ops = MultiSemantics::RiscOperators::instance(regdict);
        PartialSymbolicSemantics::RiscOperatorsPtr s1 = PartialSymbolicSemantics::RiscOperators::instance(regdict);
        SymbolicSemantics::RiscOperatorsPtr s2 = SymbolicSemantics::RiscOperators::instance(regdict);
        s2->set_compute_usedef();
        IntervalSemantics::RiscOperatorsPtr s3 = IntervalSemantics::RiscOperators::instance(regdict);
        ops->add_subdomain(s1, "PartialSymbolic");
        ops->add_subdomain(s2, "Symbolic");
        ops->add_subdomain(s3, "Interval");
        return ops;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#else
#error "invalid value for SEMANTIC_DOMAIN"
#endif

// Show the register state for BaseSemantics::RegisterStateGeneric in the same format as for RegisterStateX86. This is
// for comparison of the two register states when verifying results.  It's also close to the format used by the old binary
// semantics API.
void
show_state(const BaseSemantics::RiscOperatorsPtr &ops)
{
#if SEMANTIC_DOMAIN == MULTI_DOMAIN
    std::cout <<*ops;
    return;
#endif

    struct ShowReg {
        BaseSemantics::RiscOperatorsPtr ops;
        std::ostream &o;
        std::string prefix;

        ShowReg(const BaseSemantics::RiscOperatorsPtr &ops, std::ostream &o, const std::string &prefix)
            : ops(ops), o(o), prefix(prefix) {}

        void operator()(const char *name, const char *abbr=NULL) {
            const RegisterDictionary *regdict = ops->currentState()->registerState()->get_register_dictionary();
            const RegisterDescriptor *desc = regdict->lookup(name);
            assert(desc);
            (*this)(*desc, abbr?abbr:name);
        }
        void operator()(const RegisterDescriptor &desc, const char *abbr) {
            BaseSemantics::RegisterStatePtr regstate = ops->currentState()->registerState();
            FormatRestorer fmt(o);
            o <<prefix <<std::setw(8) <<std::left <<abbr <<"= { ";
            fmt.restore();
            BaseSemantics::SValuePtr val = regstate->readRegister(desc, ops->undefined_(desc.get_nbits()), ops.get());
            o <<*val <<" }\n";
        }
        void operator()(unsigned majr, unsigned minr, unsigned offset, unsigned nbits, const char *abbr) {
            (*this)(RegisterDescriptor(majr, minr, offset, nbits), abbr);
        }
    } show(ops, std::cout, "    ");

    std::cout <<"registers:\n";
    show("eax",         "ax");
    show("ecx",         "cx");
    show("edx",         "dx");
    show("ebx",         "bx");
    show("esp",         "sp");
    show("ebp",         "bp");
    show("esi",         "si");
    show("edi",         "di");
    show("es");
    show("cs");
    show("ss");
    show("ds");
    show("fs");
    show("gs");
    show("cf");
    show(x86_regclass_flags, 0, 1, 1, "?1");
    show("pf");
    show(x86_regclass_flags, 0, 3, 1, "?3");
    show("af");
    show(x86_regclass_flags, 0, 5, 1, "?5");
    show("zf");
    show("sf");
    show("tf");
    show("if");
    show("df");
    show("of");
    show(x86_regclass_flags, 0, 12, 1, "iopl0");
    show(x86_regclass_flags, 0, 13, 1, "iopl1");
    show("nt");
    show(x86_regclass_flags, 0, 15, 1, "?15");
    show("rf");
    show("vm");
    show(x86_regclass_flags, 0, 18, 1, "ac");
    show(x86_regclass_flags, 0, 19, 1, "vif");
    show(x86_regclass_flags, 0, 20, 1, "vip");
    show(x86_regclass_flags, 0, 21, 1, "id");
    show(x86_regclass_flags, 0, 22, 1, "?22");
    show(x86_regclass_flags, 0, 23, 1, "?23");
    show(x86_regclass_flags, 0, 24, 1, "?24");
    show(x86_regclass_flags, 0, 25, 1, "?25");
    show(x86_regclass_flags, 0, 26, 1, "?26");
    show(x86_regclass_flags, 0, 27, 1, "?27");
    show(x86_regclass_flags, 0, 28, 1, "?28");
    show(x86_regclass_flags, 0, 29, 1, "?29");
    show(x86_regclass_flags, 0, 30, 1, "?30");
    show(x86_regclass_flags, 0, 31, 1, "?31");
    show("eip", "ip");

    BaseSemantics::Formatter memfmt;
    memfmt.set_line_prefix("    ");
    std::cout <<"memory:\n";
    ops->currentState()->printMemory(std::cout, memfmt);
}


/* Analyze a single interpretation a block at a time */
static void
analyze_interp(SgAsmInterpretation *interp)
{
    /* Get the set of all instructions except instructions that are part of left-over blocks. */
    struct AllInstructions: public SgSimpleProcessing, public std::map<rose_addr_t, SgAsmX86Instruction*> {
        void visit(SgNode *node) {
            SgAsmX86Instruction *insn = isSgAsmX86Instruction(node);
            SgAsmFunction *func = SageInterface::getEnclosingNode<SgAsmFunction>(insn);
            if (func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS))
                insert(std::make_pair(insn->get_address(), insn));
        }
    } insns;
    insns.traverse(interp, postorder);

    while (!insns.empty()) {
        std::cout <<"=====================================================================================\n"
                  <<"=== Starting a new basic block                                                    ===\n"
                  <<"=====================================================================================\n";
        AllInstructions::iterator si = insns.begin();
        SgAsmX86Instruction *insn = si->second;
        insns.erase(si);

        BaseSemantics::RiscOperatorsPtr operators = make_ops();
        BaseSemantics::Formatter formatter;
        formatter.set_suppress_initial_values();
        formatter.set_show_latest_writers(do_usedef);
        BaseSemantics::DispatcherPtr dispatcher;
        if (do_trace) {
            // Enable RiscOperators tracing, but turn off a bunch of info that makes comparisons with a known good answer
            // difficult.
            Sawyer::Message::PrefixPtr prefix = Sawyer::Message::Prefix::instance();
            prefix->showProgramName(false);
            prefix->showThreadId(false);
            prefix->showElapsedTime(false);
            prefix->showFacilityName(Sawyer::Message::Prefix::NEVER);
            prefix->showImportance(false);
            Sawyer::Message::UnformattedSinkPtr sink = Sawyer::Message::StreamSink::instance(std::cout);
            sink->prefix(prefix);
            sink->defaultPropertiesNS().useColor = false;
            TraceSemantics::RiscOperatorsPtr trace = TraceSemantics::RiscOperators::instance(operators);
            trace->stream().destination(sink);
            trace->stream().enable();
            dispatcher = DispatcherX86::instance(trace, 32);
        } else {
            dispatcher = DispatcherX86::instance(operators, 32);
        }
        operators->solver(make_solver());

        // The fpstatus_top register must have a concrete value if we'll use the x86 floating-point stack (e.g., st(0))
        if (const RegisterDescriptor *REG_FPSTATUS_TOP = regdict->lookup("fpstatus_top")) {
            BaseSemantics::SValuePtr st_top = operators->number_(REG_FPSTATUS_TOP->get_nbits(), 0);
            operators->writeRegister(*REG_FPSTATUS_TOP, st_top);
        }

#if SEMANTIC_DOMAIN == SYMBOLIC_DOMAIN
        BaseSemantics::SValuePtr orig_esp;
        if (do_test_subst) {
            // Only request the orig_esp if we're going to use it later because it causes an esp value to be instantiated
            // in the state, which is printed in the output, and thus changes the answer.
            BaseSemantics::RegisterStateGeneric::promote(operators->currentState()->registerState())->initialize_large();
            orig_esp = operators->readRegister(*regdict->lookup("esp"));
            std::cout <<"Original state:\n" <<*operators;
        }
#endif

        /* Perform semantic analysis for each instruction in this block. The block ends when we no longer know the value of
         * the instruction pointer or the instruction pointer refers to an instruction that doesn't exist or which has already
         * been processed. */
        while (1) {
            /* Analyze current instruction */
            std::cout <<"\n" <<unparseInstructionWithAddress(insn) <<"\n";
            try {
                dispatcher->processInstruction(insn);
#   if 0 /*DEBUGGING [Robb P. Matzke 2013-05-01]*/
                show_state(operators); // for comparing RegisterStateGeneric with the old RegisterStateX86 output
#   else
                std::cout <<(*operators + formatter);
#   endif
            } catch (const BaseSemantics::Exception &e) {
                std::cout <<e <<"\n";
            }

            /* Never follow CALL instructions */
            if (insn->get_kind()==x86_call || insn->get_kind()==x86_farcall)
                break;

            /* Get next instruction of this block */
            BaseSemantics::SValuePtr ip = operators->readRegister(dispatcher->findRegister("eip"));
            if (!ip->is_number())
                break;
            rose_addr_t next_addr = ip->get_number();
            si = insns.find(next_addr);
            if (si==insns.end()) break;
            insn = si->second;
            insns.erase(si);
        }

        // Test substitution on the symbolic state.
#if SEMANTIC_DOMAIN == SYMBOLIC_DOMAIN
        if (do_test_subst) {
            SymbolicSemantics::SValuePtr from = SymbolicSemantics::SValue::promote(orig_esp);
            BaseSemantics::SValuePtr newvar = operators->undefined_(32);
            newvar->set_comment("frame_pointer");
            SymbolicSemantics::SValuePtr to =
                SymbolicSemantics::SValue::promote(operators->add(newvar, operators->number_(32, 4)));
            std::cout <<"Substituting from " <<*from <<" to " <<*to <<"\n";
            SymbolicSemantics::RiscOperators::promote(operators)->substitute(from, to);
            std::cout <<"Substituted state:\n" <<(*operators+formatter);
        }
#endif
    }
}

/* Analyze only interpretations that point only to 32-bit x86 instructions. */
class AnalyzeX86Functions: public SgSimpleProcessing {
public:
    size_t ninterps;
    AnalyzeX86Functions(): ninterps(0) {}
    void visit(SgNode* node) {
        SgAsmInterpretation *interp = isSgAsmInterpretation(node);
        if (interp) {
            const SgAsmGenericHeaderPtrList &headers = interp->get_headers()->get_headers();
            bool only_x86 = true;
            for (size_t i=0; i<headers.size() && only_x86; ++i)
                only_x86 = 4==headers[i]->get_word_size();
            if (only_x86) {
                ++ninterps;
                analyze_interp(interp);
            }
        }
    }
};

int main(int argc, char *argv[]) {
    std::vector<std::string> args(argv, argv+argc);
    for (size_t argno=1; argno<args.size(); ++argno) {
        if (0==args[argno].compare("--trace")) {
            do_trace = true;
            args[argno] = "";
        } else if (0==args[argno].compare("--no-trace")) {
            do_trace = false;
            args[argno] = "";
        } else if (0==args[argno].compare("--test-subst")) {
            do_test_subst = true;
            args[argno] = "";
        } else if (0==args[argno].compare("--no-test-subst")) {
            do_test_subst = false;
            args[argno] = "";
        } else if (0==args[argno].compare("--usedef")) {
            do_usedef = true;
            args[argno] = "";
        } else if (0==args[argno].compare("--no-usedef")) {
            do_usedef = false;
            args[argno] = "";
        }
    }
    args.erase(std::remove(args.begin(), args.end(), ""), args.end());

    SgProject *project = frontend(argc, argv);
    AnalyzeX86Functions analysis;
    analysis.traverse(project, postorder);
    if (0==analysis.ninterps) {
        std::cout <<"file(s) didn't have any 32-bit x86 headers.\n";
    } else {
        std::cout <<"analyzed headers: " <<analysis.ninterps<< "\n";
    }

    std::ostream &info = std::cerr; // do not include in answer because objects vary in size per architecture
    info <<"Before vacuum...\n";
    BaseSemantics::SValue::poolAllocator().showInfo(info);
    BaseSemantics::SValue::poolAllocator().vacuum();
    info <<"After vacuum...\n";
    BaseSemantics::SValue::poolAllocator().showInfo(info);

    return 0;
}

#endif
