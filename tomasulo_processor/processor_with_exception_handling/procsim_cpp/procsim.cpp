#include "procsim.hpp"

using namespace std;

// some global variables
uint64_t m_r, m_f, m_k0, m_k1, m_k2, m_e, m_s;     // copying system parameters 
uint64_t cycle_number = 1;     // cycle number
uint32_t g_inst_number = 1;       // instruction number = line number of trace file
uint32_t retired_inst_count = 0;        // for actual counting purposes = 100000
uint32_t fired_inst_count = 0;
int32_t retired_inst_number = 0;
int32_t retired_inst_number_pr = 0;
uint32_t g_tag = 0;             // generating tags for instructions sequentially
std::vector<uint32_t> dispatchQ_size;
int32_t pc = 0;        // program counter = inst_number - 1
int32_t return_pc = 0;
int32_t IB0 = 0;
int32_t IB1 = 19;
int32_t IB2 = 0;
bool exceptionBit = 0;
std::vector<int32_t> exception_PC;      // stores all PCs of all exceptions handled
int32_t regfile_hits = 0;
int32_t ROB_hits = 0;
std::vector<int32_t> flushed_inst;
int32_t backup_count = 0;
uint32_t latestinSQ;    // PC


// all local structures
struct s_fetch {
    std::vector<uint32_t> m_instruction_address;
        std::vector<int32_t> m_op_code;
        std::vector<int32_t> m_src_reg_0;
        std::vector<int32_t> m_src_reg_1;
        std::vector<int32_t> m_dest_reg;
        std::vector<int32_t> inst_number;
        std::vector<int32_t> retired;
        int32_t empty;  // empty = 1 -> fetch is empty (beginning of program), empty = 0 -> buffer is full
};

struct s_dispatchQ {
        std::vector<int32_t> op_code;
        std::vector<int32_t> src_reg_1;
        std::vector<int32_t> src1_tag;
        std::vector<int32_t> src1_ready;
        std::vector<int32_t> src_reg_2;
        std::vector<int32_t> src2_tag;
        std::vector<int32_t> src2_ready;
        std::vector<int32_t> dest_reg;
        std::vector<int32_t> dest_tag;
        std::vector<int32_t> inst_number;
        int32_t empty;  // empty = 1 -> dispatch queue is empty (beginning of program), empty = 0 -> queue is full
};

struct s_schedQ {
        uint32_t size;
        std::vector<int32_t> valid;     // 0 means empty entry, 1 means valid
        std::vector<int32_t> inst_number;
        std::vector<int32_t> FU;
        std::vector<bool> FU_assigned;  // 0 means not assigned, 1 means FU is assigned
        std::vector<int32_t> dest_reg;
        std::vector<int32_t> dest_tag;
        std::vector<int32_t> src1_ready;
        std::vector<int32_t> src1_tag;
        std::vector<int32_t> src2_ready;
        std::vector<int32_t> src2_tag;
        std::vector<bool> srcreg_ready;
        std::vector<bool> completed;    // 1 indicates instruction is ready to be completed and ready to be updated on the CDB
        std::vector<bool> retired;      // 1 indicates instruction is ready to be retired and can be kicked out of scheduling queue
        std::vector<int32_t> broadcastQ_no;        // stores location of instruction in SQ in order of CDB broadcast
        std::vector<bool> inBQ; // 1 indicates instruction is in BQ
        std::vector<int32_t> FUQ_no;    // stores location of instruction in SQ in order of assigning them FUs
        std::vector<bool> inFUQ;    // 1 indicates instruction is in FUQ
        std::vector<bool> exception;    // 1 indicates instruction will throw an exception     
};

struct s_scoreboard {
        // 0 means free, 1 means busy
        std::vector<int32_t> FU_k0;
        std::vector<int32_t> FU_k1;
        std::vector<int32_t> FU_k2;
        std::vector<int32_t> tag_k0;
        std::vector<int32_t> tag_k1;
        std::vector<int32_t> tag_k2;          
};

struct s_cdb {
        std::vector<int32_t> cdb_tag;
        std::vector<int32_t> cdb_destreg;
        std::vector<bool> cdb_busy; // 0 = not busy, 1 = busy        
};

struct s_ROB {
        uint32_t size;
        std::vector<int32_t> dest_tag;
        std::vector<int32_t> dest_reg;
        std::vector<bool> ready;        // 1 means ready, 0 otherwise
        std::vector<bool> exception;    // 1 means exceptiom, 0 otherwise. Updated only at ROB head upon retirement
        std::vector<int32_t> PC;        // indicates the PC of the instruction = inst_number - 1
        std::vector<bool> retired;      // 0 is yet to retire, 1 is ready to retire, will be deleted in next cycle
        bool exceptionBit = 0;  // 1 indicates it's an exception, goes high in next cycle portion 1
};

// register file
struct s_regfile {
        int32_t reg_ready[128];
        int32_t reg_tag[128];
        
        int32_t B1_reg_ready[128];
        int32_t B1_reg_tag[128];
        int32_t B2_reg_ready[128];
        int32_t B2_reg_tag[128];
};


// result cycles of all instructions
// common notation = use inst_number - 1 to access the corresp inst stats
int32_t result_fetched[100000];
int32_t result_dispatched[100000];
int32_t result_scheduled[100000];
int32_t result_executed[100000];
int32_t result_broadcasted[100000];
int32_t result_deleted[100000];


// all local functions used
void print_results();
void print_SQ(s_schedQ* p_schedQ);
void print_FU(s_scoreboard* p_scoreboard);
void print_CDB(s_cdb* p_cdb);
void print_regfile(s_regfile* p_regfile);
void print_DQ(s_dispatchQ* p_dispatchQ);
void print_FUQ(s_schedQ* p_schedQ);
void print_BQ(s_schedQ* p_schedQ);
void print_ROB(s_ROB* p_ROB);
void print_exceptionPC();
void init(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB);
void fetch(proc_inst_t* p_inst, s_fetch* p_fetch);
void dispatch(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_regfile* p_regfile, s_ROB* p_ROB);
void rm_retired(s_fetch* p_fetch, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_ROB* p_ROB, s_regfile* p_regfile);
void schedule(s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB);
void execute(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile);
void FU_assign(int32_t index, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard);
void retire(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB);
void ROB_regfile(s_dispatchQ* p_dispatchQ, s_ROB* p_ROB, s_regfile* p_regfile);
bool stopping_condn(s_fetch* p_fetch);

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r Number of result buses
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t e, uint64_t s)
{
        // all local variables used here
        int32_t i;
       
    // initializing variables being used
    m_r = r;     // number of result buses
    m_f = f;    // number of instructions fetched
    m_k0 = k0;    // number of k0 FUs
    m_k1 = k1;    // number of k1 FUs
    m_k2 = k2;     // number of k2 FUs
    m_e = e;
    m_s = s;
   
    for (i = 0; i < 100000; i++)
    {
            result_fetched[i] = -1;
            result_dispatched[i] = -1;
            result_scheduled[i] = -1;
            result_executed[i] = -1;
            result_broadcasted[i] = -1;
            result_deleted[i] = -1;
    }
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void run_proc(proc_stats_t* p_stats, proc_inst_t* p_inst)
{
    s_fetch m_fetch;
    s_dispatchQ m_dispatchQ;
    s_schedQ m_schedQ;
    s_scoreboard m_scoreboard;
    s_cdb m_cdb;
    s_regfile m_regfile;
    s_ROB m_ROB;
   
    init(&m_fetch, &m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile, &m_ROB);    // initializing any parameters to be used
        fetch(p_inst, &m_fetch);
    // this for loop has to be replaced with retire instruction condition
    //for(int32_t i = 0; i < 58061; i++)
        if(m_s == 1)    // ROB
        {
                while(retired_inst_number != 100000)
                {
                        retire(&m_fetch, &m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile, &m_ROB);
                        execute(&m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);
                        schedule(&m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile, &m_ROB);
                        rm_retired(&m_fetch, &m_schedQ, &m_scoreboard, &m_ROB, &m_regfile);
                        dispatch(&m_fetch, &m_dispatchQ, &m_regfile, &m_ROB); 
                        cycle_number++;   
                }  
        }
        
        if(m_s == 2)    // CPR
        {
                while(stopping_condn(&m_fetch))
                {
                        retire(&m_fetch, &m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile, &m_ROB);
                        execute(&m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);
                        schedule(&m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile, &m_ROB);
                        rm_retired(&m_fetch, &m_schedQ, &m_scoreboard, &m_ROB, &m_regfile);
                        dispatch(&m_fetch, &m_dispatchQ, &m_regfile, &m_ROB); 
                        cycle_number++;   
                }  
        }

        /*print_DQ(&m_dispatchQ);
        print_SQ(&m_schedQ);
        print_ROB(&m_ROB);
        print_FU(&m_scoreboard);
        print_CDB(&m_cdb);
        print_regfile(&m_regfile);
        print_results();
        print_FUQ(&m_schedQ);
        print_BQ(&m_schedQ);
        */
    
        print_results();
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats)
{
       
        // total number of instructions in the trace
        p_stats->retired_instruction = g_inst_number - 1;
        
        // average dispatch queue size and max dispatch queue size
        uint32_t i;
        uint32_t sum = 0;
        uint32_t max = 0;
        for(i = 0; i < dispatchQ_size.size(); i++)
        {
                sum += dispatchQ_size[i];
                if(dispatchQ_size[i] > max)
                {
                        max = dispatchQ_size[i];
                }       
        }
        
        p_stats->avg_disp_size = (double) sum / (double) (cycle_number - 1);
        p_stats->max_disp_size = max;
        
        // average number of instructions fired per cycle
        p_stats->avg_inst_fired = (double) (fired_inst_count) / (double) (cycle_number - 1);
        
        // average number of instructions retired per cycle
        p_stats->avg_inst_retired = (double) (100000) / (double) (cycle_number - 1);
         
        // total run time
        p_stats->cycle_count = cycle_number - 1;
        
        // Regfile hits
        p_stats->reg_file_hit_count = regfile_hits;
        
        // ROB hits
        p_stats->rob_hit_count = ROB_hits;
        
        // total exceptions
        p_stats->exception_count = exception_PC.size();
        
        // total flushed instructions
        int32_t flushed_inst_count = 0;
        for(i = 0; i < flushed_inst.size(); i++)
        {
                flushed_inst_count += flushed_inst[i];
                //cout<<"\t"<<flushed_inst_count;
        }
         p_stats->flushed_count = flushed_inst_count;
         
         // total number of backups
         p_stats->backup_count = backup_count;
}

/**
 * Subroutine to print stats of each instruction every cycle
 */
void print_results()
{
        cout<<"INST	FETCH	DISP	SCHED	EXEC	STATE";
        for (int32_t i = 0; i < 100000; i++)
        {
                cout<<"\n"<<(i+1)<<"\t"<<result_fetched[i]<<"\t"<<(result_fetched[i] + 1)<<"\t"<<(result_dispatched[i] + 1)<<"\t"<<(result_scheduled[i] + 1)<<"\t"<<result_deleted[i];
        }
}

/**
 * DEBUG: print scheduling queue
 */
void print_SQ(s_schedQ* p_schedQ)
{
        // all local variables
        uint32_t i;
       
        cout<<"\nScheduling Queue:";
        cout<<"\nINST\tDEST\tSRC1_TAG\tSRC1_READY\tSRC2_TAG\tSRC2_READY\tSRCREG_READY\tFU_ASG\tCOMP\tRET\tEXCP";
        for(i = 0; i < (p_schedQ->size); i++)
        {
                cout<<"\n"<<(p_schedQ->inst_number)[i]<<"("<<(p_schedQ->valid)[i]<<")\t"<<(p_schedQ->dest_tag)[i]<<"\t"<<(p_schedQ->src1_tag)[i]<<"\t\t"<<(p_schedQ->src1_ready)[i]<<"\t\t"<<(p_schedQ->src2_tag)[i]<<"\t\t"<<(p_schedQ->src2_ready)[i]<<"\t\t"<<(p_schedQ->srcreg_ready)[i]<<"\t\t"<<(p_schedQ->FU_assigned)[i]<<"\t"<<(p_schedQ->completed)[i]<<"\t"<<(p_schedQ->retired)[i]<<"\t"<<(p_schedQ->exception)[i];
        }
}

/**
 * DEBUG: print ROB
 */
void print_ROB(s_ROB* p_ROB)
{
        // all local variables
        uint32_t i;
       
        if((p_ROB->PC).size() != 0)
        {
                cout<<"\nROB:";
                cout<<"\nPC\tDEST_REG\tDEST_TAG\tREADY\tRET\tEXCEP";
                for(i = 0; i < (p_ROB->PC).size(); i++)
                {
                        cout<<"\n"<<(p_ROB->PC)[i]<<"\t"<<(p_ROB->dest_reg)[i]<<"\t\t"<<(p_ROB->dest_tag)[i]<<"\t\t"<<(p_ROB->ready)[i]<<"\t"<<(p_ROB->retired)[i]<<"\t"<<(p_ROB->exception)[i];
                }
        }
}


/**
 * DEBUG: print functional units
 */
void print_FU(s_scoreboard* p_scoreboard)
{
        // all local variables
        uint32_t i;
       
        cout<<"\nFunctional Unit:";
        cout<<"\nFU0\t";
        for(i = 0; i < m_k0; i++)
        {
                cout<<"\t"<<(p_scoreboard->tag_k0)[i]<<"("<<(p_scoreboard->FU_k0)[i]<<")";
        }
        cout<<"\nFU1\t";
        for(i = 0; i < m_k1; i++)
        {
                cout<<"\t"<<(p_scoreboard->tag_k1)[i]<<"("<<(p_scoreboard->FU_k1)[i]<<")";
        }
        cout<<"\nFU2\t";
        for(i = 0; i < m_k2; i++)
        {
                cout<<"\t"<<(p_scoreboard->tag_k2)[i]<<"("<<(p_scoreboard->FU_k2)[i]<<")";
        }
}

/**
 * DEBUG: print CDB
 */
void print_CDB(s_cdb* p_cdb)
{
        // all local variables
        uint32_t i;
       
        cout<<"\nCDB:";
        for(i = 0; i < m_r; i++)
        {
                cout<<"\t"<<(p_cdb->cdb_tag)[i]<<"("<<(p_cdb->cdb_busy)[i]<<")";
        }       
}

/**
 * DEBUG: print reg file
 */
void print_regfile(s_regfile* p_regfile)
{
        // all local variables
        uint32_t i;
       
        cout<<"\nRegister File:";
        cout<<"\nREG\tREADY\tTAG";
        for(i = 0; i < 32; i++)
        {
                cout<<"\n"<<i<<"\t"<<(p_regfile->reg_ready)[i]<<"\t"<<(p_regfile->reg_tag)[i];
        }       
}

/**
 * DEBUG: print dispatch queue
 */
void print_DQ(s_dispatchQ* p_dispatchQ)
{
        cout<<"\nDQ:\n";
        cout<<"DEST_TAG\tDEST_REG\tSRC_REG_1\tSRC1_READY\tSRC1_TAG\tSRC_REG_2\tSRC2_READY\tSRC2_TAG\n";
        for(uint32_t i = 0; i < (p_dispatchQ->inst_number).size(); i++)
        {
                cout<<"\n"<<(p_dispatchQ->dest_tag)[i]<<"\t\t"<<(p_dispatchQ->dest_reg)[i]<<"\t\t"<<(p_dispatchQ->src_reg_1)[i]<<"\t\t"<<(p_dispatchQ->src1_ready)[i]<<"\t\t"<<(p_dispatchQ->src1_tag)[i]<<"\t\t"<<(p_dispatchQ->src_reg_2)[i]<<"\t\t"<<(p_dispatchQ->src2_ready)[i]<<"\t\t"<<(p_dispatchQ->src2_tag)[i];
        }
}

/**
 * DEBUG: print FU queue
 */
void print_FUQ(s_schedQ* p_schedQ)
{
        cout<<"\nFUQ:\n";
        if((p_schedQ->FUQ_no).size() != 0)
        {
                for(uint32_t i = 0; i < (p_schedQ->FUQ_no).size(); i++)
                {
                        cout<<"\t"<<(p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];//<<"("<<(p_schedQ->FUQ_cycle)[i]<<")";
                }
        }       
}

/**
 * DEBUG: print broadcast queue
 */
void print_BQ(s_schedQ* p_schedQ)
{
        cout<<"\nBQ:\n";
        if((p_schedQ->broadcastQ_no).size() != 0)
        {
                for(uint32_t i = 0; i < (p_schedQ->broadcastQ_no).size(); i++)
                {
                        cout<<"\t"<<(p_schedQ->dest_tag)[(p_schedQ->broadcastQ_no)[i]];
                }
        } 
}

void print_exceptionPC()
{
        cout<<"\nException queue:\n";
        if(exception_PC.size() != 0)
        {
                for(uint32_t i = 0; i < exception_PC.size(); i++)
                {
                        cout<<"\t"<<exception_PC[i];
                }
        }         
}

/**
 * Subroutine to initialize all counters and parameters
 */
void init(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB)
{
        uint32_t i;
      
        p_fetch->empty = 1;
        p_dispatchQ->empty = 1;
        p_schedQ->size = 2 * (m_k0 + m_k1 + m_k2);
        p_ROB->size = p_schedQ->size;
       
        // create and initialize all vectors not accessed by push_back()
        (p_schedQ->valid).assign((p_schedQ->size), 0);     // 0 means empty entry, 1 means valid
        (p_schedQ->inst_number).assign((p_schedQ->size), 0);
        (p_schedQ->FU).assign((p_schedQ->size), 0);
        (p_schedQ->FU_assigned).assign((p_schedQ->size), 0);
        (p_schedQ-> dest_reg).assign((p_schedQ->size), 0);
        (p_schedQ->dest_tag).assign((p_schedQ->size), 0);
        (p_schedQ->src1_ready).assign((p_schedQ->size), 0);
        (p_schedQ->src1_tag).assign((p_schedQ->size), 0);
        (p_schedQ->src2_ready).assign((p_schedQ->size), 0);
        (p_schedQ->src2_tag).assign((p_schedQ->size), 0);
        (p_schedQ->srcreg_ready).assign((p_schedQ->size), 0);
        (p_schedQ->completed).assign((p_schedQ->size), 0);
        (p_schedQ->retired).assign((p_schedQ->size), 0);
        (p_schedQ->inBQ).assign((p_schedQ->size), 0);
        (p_schedQ->inFUQ).assign((p_schedQ->size), 0);
        (p_schedQ->exception).assign((p_schedQ->size), 0);
   
        (p_scoreboard->FU_k0).assign(m_k0, 0);
        (p_scoreboard->FU_k1).assign(m_k1, 0);
        (p_scoreboard->FU_k2).assign(m_k2, 0);
        (p_scoreboard->tag_k0).assign(m_k0, 0);
        (p_scoreboard->tag_k1).assign(m_k1, 0);
        (p_scoreboard->tag_k2).assign(m_k2, 0);
       
        (p_cdb->cdb_destreg).assign(m_r, 0);
        (p_cdb->cdb_tag).assign(m_r, 0);
        (p_cdb->cdb_busy).assign(m_r, 0);
       
        for(i = 0; i < 128; i++)
        {
                (p_regfile->reg_ready)[i] = 1;
                (p_regfile->reg_tag)[i] = 100000 + i;
                (p_regfile->B1_reg_ready)[i] = 1;
                (p_regfile->B1_reg_tag)[i] = 100000 + i;
                (p_regfile->B2_reg_ready)[i] = 1;
                (p_regfile->B2_reg_tag)[i] = 100000 + i;
                
        } 
}

/**
 * Subroutine to make the stopping condition
 * This checks if all instructions are done retiring 
 */
bool stopping_condn(s_fetch* p_fetch)
{
        int32_t i;
        
        if((retired_inst_number <= 100000) && (retired_inst_number >= 99985)) 
        {
                for(i = 99980; i < 100000; i++)
                {
                        if((p_fetch->retired)[i] == 0)
                        {
                                return true;
                        }
                }
                return false;
        }
        else
                return true;
        
                
        
}


/**
 * Subroutine to fetch F instructions per cycle.
 * Fetched instructions are stored in fetch buffer, which essentially is like the entire memory 
 */
void fetch(proc_inst_t* p_inst, s_fetch* p_fetch)
{
    // variables used in fetch()
    uint32_t i;
   
//    if((p_fetch->m_instruction_address).size() == 0)
//       p_fetch->empty = 1;  
       
    // fetching f instructions in one cycle
    for (i = 0; i < 100000; i++)
    {
        if (read_instruction(p_inst) == true)
        {
                (p_fetch->m_instruction_address).push_back(p_inst->instruction_address);           
                (p_fetch->m_op_code).push_back(p_inst->op_code);
                (p_fetch->m_src_reg_0).push_back(p_inst->src_reg[0]);
                (p_fetch->m_src_reg_1).push_back(p_inst->src_reg[1]);
                (p_fetch->m_dest_reg).push_back(p_inst->dest_reg);
                (p_fetch->inst_number).push_back(g_inst_number);
                (p_fetch->retired).push_back(0);

                // update stats
                p_fetch->empty = 0;     // not empty anymore
                                  
                g_inst_number++;      
        }   
    }      
}

/**
 * Subroutine to put newly fetched instructions into the dispatch queue
 * Read from the ROB and if not found, update from register file and update src1 and src2 states
 * Update regfile with dest reg of instructions stats
 * Update DISP state
 */
void dispatch(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_regfile* p_regfile, s_ROB* p_ROB)
{
        // variables used in dispatch()
        uint32_t i, j;
        //(p_dispatchQ->empty) = 1;
        // put in dispatch queue only if fetch is not empty
                
        if(m_s == 1)
        {
                if(((p_ROB->exceptionBit) == 1) && ((p_fetch->empty) == 0))
                {
                        (p_ROB->exceptionBit) = 0; 
                        flushed_inst.push_back(latestinSQ-return_pc+1);
                        pc = return_pc;
                }

                else if(((p_fetch->empty) == 0) && (pc < 100000))
                {  
                        if((p_ROB->exceptionBit) == 0)   
                        {
                                // fetching f instructions in one cycle
                                for (i = 0; i < m_f; i++)
                                {  
                                        int32_t flag1 = 0;
                                        int32_t flag2 = 0; 
                                        //print_DQ(p_dispatchQ);
                                        result_fetched[pc] = cycle_number;
                                
                                        (p_dispatchQ->op_code).push_back((p_fetch->m_op_code)[pc]);
                                        (p_dispatchQ->src_reg_1).push_back((p_fetch->m_src_reg_0)[pc]);
                                        (p_dispatchQ->src_reg_2).push_back((p_fetch->m_src_reg_1)[pc]);
                                        (p_dispatchQ->dest_reg).push_back((p_fetch->m_dest_reg)[pc]);
                                        (p_dispatchQ->inst_number).push_back((p_fetch->inst_number)[pc]);
                                        (p_dispatchQ->dest_tag).push_back(pc);
                                               
                                        // first search in ROB and then reg file for src1 and src2 tags and ready bits
                                        if((p_fetch->m_src_reg_0)[pc] == -1)
                                        {
                                                (p_dispatchQ->src1_ready).push_back(1);
                                                (p_dispatchQ->src1_tag).push_back(-1); 
                                                flag1 = 1;
                                                //cout<<"\n1.1";
                                        }
                                        else 
                                        {
                                                (p_dispatchQ->src1_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_0)[pc]]);  
                                                // search in ROB
                                                if((p_ROB->dest_tag).size() != 0)
                                                {
                                                        for(j = (p_ROB->dest_tag).size(); j > 0 ; j--)
                                                        {
                                                                if((p_ROB->dest_tag)[j-1] == (p_regfile->reg_tag)[(p_fetch->m_src_reg_0)[pc]] && (flag1 == 0))
                                                                {
                                                                        if((p_ROB->ready)[j-1] == 1)
                                                                        {
                                                                                (p_dispatchQ->src1_ready).push_back(1);
                                                                                flag1 = 1;
                                                                                //cout<<"\n1.2";
                                                                        }
                                                                        else if((p_ROB->ready)[j-1] == 0)
                                                                        {
                                                                                (p_dispatchQ->src1_ready).push_back(0);
                                                                                flag1 = 1;  
                                                                                //cout<<"\n1.3";
                                                                        }                                            
                                                                }
                                                        }
                                                }
                                                // search in RF
                                                if(flag1 == 0)
                                                {
                                                        if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[pc]] == 1)
                                                        {
                                                                (p_dispatchQ->src1_ready).push_back(1);
                                                                flag1 = 1;
                                                                //cout<<"\n1.4";
                                                        }
                                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[pc]] == 0)
                                                        {
                                                                (p_dispatchQ->src1_ready).push_back(0);
                                                                flag1 = 1; 
                                                                //cout<<"\n1.5";
                                                        }                                
                                                }                                                     
                                        }
                                        
                                        if((p_fetch->m_src_reg_1)[pc] == -1)
                                        {
                                                (p_dispatchQ->src2_ready).push_back(1);
                                                (p_dispatchQ->src2_tag).push_back(-1);
                                                flag2 = 1;
                                                //cout<<"\n2.1";
                                        }
                                        else 
                                        {
                                                (p_dispatchQ->src2_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_1)[pc]]);  
                                                // search in ROB
                                                if((p_ROB->dest_tag).size() != 0)
                                                {
                                                        for(j = (p_ROB->dest_tag).size(); j > 0 ; j--)
                                                        {
                                                                
                                                                if((p_ROB->dest_tag)[j-1] == (p_regfile->reg_tag)[(p_fetch->m_src_reg_1)[pc]] && (flag2 == 0))
                                                                {   
                                                                        if((p_ROB->ready)[j-1] == 1)
                                                                        {
                                                                                (p_dispatchQ->src2_ready).push_back(1);
                                                                                flag2 = 1;
                                                                                //cout<<"\n2.2";
                                                                        }
                                                                        else if((p_ROB->ready)[j-1] == 0)
                                                                        {
                                                                                (p_dispatchQ->src2_ready).push_back(0);
                                                                                flag2 = 1;  
                                                                                //cout<<"\n2.3";
                                                                        }                                            
                                                                }
                                                        }
                                                }
                                                // search in RF
                                                if(flag2 == 0)
                                                {
                                                        if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[pc]] == 1)
                                                        {
                                                                (p_dispatchQ->src2_ready).push_back(1);
                                                                flag2 = 1;
                                                                //cout<<"\n2.4";
                                                        }
                                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[pc]] == 0)
                                                        {
                                                                (p_dispatchQ->src2_ready).push_back(0);
                                                                flag2 = 1; 
                                                                //cout<<"\n2.5";
                                                        }                                
                                                }                                                     
                                        }
                                      
                                        // reserve dest reg file
                                        if((p_fetch->m_dest_reg)[pc] != -1)
                                        {
                                                (p_regfile->reg_ready)[(p_fetch->m_dest_reg)[pc]] = 0;
                                                (p_regfile->reg_tag)[(p_fetch->m_dest_reg)[pc]] = pc;
                                        }   
                                       
                                        // update stats                   
                                        pc++;
                                        if(pc == 100000)
                                                break;
                                }   
                                p_dispatchQ->empty = 0;     // not empty anymore 
                        }
                }
                 
        }
        else if (m_s == 2)
        {
                if((exceptionBit == 1) && ((p_fetch->empty) == 0))
                {
                        exceptionBit = 0;
                        flushed_inst.push_back(latestinSQ-IB2+1); 
                        //cout<<"\nFlushed "<<(latestinSQ-IB2+1);
                        pc = IB2;
                        //cout<<"\nNew PC: "<<pc;
                }

                else if(((p_fetch->empty) == 0) && (pc < 100000))
                {       
                        if(exceptionBit == 0)  
                        {
                                // fetching f instructions in one cycle
                                for (i = 0; i < m_f; i++)
                                {  
                                        result_fetched[pc] = cycle_number;  
                                          
                                        (p_dispatchQ->op_code).push_back((p_fetch->m_op_code)[pc]);
                                        (p_dispatchQ->src_reg_1).push_back((p_fetch->m_src_reg_0)[pc]);
                                        (p_dispatchQ->src_reg_2).push_back((p_fetch->m_src_reg_1)[pc]);
                                        (p_dispatchQ->dest_reg).push_back((p_fetch->m_dest_reg)[pc]);
                                        (p_dispatchQ->inst_number).push_back((p_fetch->inst_number)[pc]);
                                        (p_dispatchQ->dest_tag).push_back(pc);
                                        
                                        (p_fetch->retired)[pc] = 0;
                                               
                                        // search in reg file for src1 and src2 tags and ready bits
                                        // for src1
                                        if((p_fetch->m_src_reg_0)[pc] == -1)
                                        {
                                                (p_dispatchQ->src1_ready).push_back(1);
                                                (p_dispatchQ->src1_tag).push_back(-1);
                                        }
                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[pc]] == 1)
                                        {
                                                (p_dispatchQ->src1_ready).push_back(1);
                                                (p_dispatchQ->src1_tag).push_back(-1);
                                        }
                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[pc]] == 0)
                                        {
                                                (p_dispatchQ->src1_ready).push_back(0);
                                                (p_dispatchQ->src1_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_0)[pc]]); 
                                        }

                                        // for src2
                                        if((p_fetch->m_src_reg_1)[pc] == -1)
                                        {
                                                (p_dispatchQ->src2_ready).push_back(1);
                                                (p_dispatchQ->src2_tag).push_back(-1);
                                        }
                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[pc]] == 1)
                                        {
                                                (p_dispatchQ->src2_ready).push_back(1);
                                                (p_dispatchQ->src2_tag).push_back(-1);
                                        }
                                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[pc]] == 0)
                                        {
                                                (p_dispatchQ->src2_ready).push_back(0);                                                
                                                (p_dispatchQ->src2_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_1)[pc]]);
                                        }

                                        // reserve dest reg file
                                        if((p_fetch->m_dest_reg)[pc] != -1)
                                        {
                                                (p_regfile->reg_ready)[(p_fetch->m_dest_reg)[pc]] = 0;
                                                (p_regfile->reg_tag)[(p_fetch->m_dest_reg)[pc]] = pc;
                                                
                                                if(pc <= IB1)
                                                {
                                                        (p_regfile->B1_reg_ready)[(p_fetch->m_dest_reg)[pc]] = 0;
                                                        (p_regfile->B1_reg_tag)[(p_fetch->m_dest_reg)[pc]] = pc;
                                                }
                                        }   
                                        //print_DQ(p_dispatchQ);
                                        // update stats                   
                                        pc++;
                                        if(pc == 100000)
                                                break;
                                }   
                                p_dispatchQ->empty = 0;     // not empty anymore            
                        }                
                }
        }  
          
}

/**
 * Subroutine to delete retired instructions from the scheduling queue and ROB head
 */
void rm_retired(s_fetch* p_fetch, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_ROB* p_ROB, s_regfile* p_regfile)
{
        // all local variables
        uint32_t i, j;
        if(m_s == 1)
        {
                if((p_ROB->retired).size() != 0)       
                {
                        uint32_t a = (p_ROB->PC).size();
                        for(j = 0; j < a; j++)
                        {
                                if((p_ROB->retired)[0] == 1)
                                {
                                        for(i = 0; i < (p_schedQ->size); i++)
                                        {
                                                if(((p_schedQ->inst_number)[i] == ((p_ROB->PC)[0] + 1)) && ((p_schedQ->valid)[i] == 1))
                                                {
                                                        // update stats
                                                        //result_deleted[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                                                        //retired_inst_count++;
                                                        
                                                        // update RF
                                                        if((p_ROB->dest_tag)[0] == (p_regfile->reg_tag)[(p_ROB->dest_reg)[0]])
                                                        {
                                                                (p_regfile->reg_ready)[(p_ROB->dest_reg)[0]] = 1;
                                                        }
                                                        
                                                        (p_schedQ->valid)[i] = 0;
                                                        (p_schedQ->FU_assigned)[i] = 0;
                                                        (p_schedQ->srcreg_ready)[i] = 0;
                                                        (p_schedQ->completed)[i] = 0;   
                                                        (p_schedQ->retired)[i] = 0;
                                                        (p_schedQ->inBQ)[i] = 0;
                                                        (p_schedQ->inFUQ)[i] = 0;  
                                                        (p_schedQ->exception)[i] = 0;           
                                                }
                                        }
                                        
                                        (p_ROB->dest_tag).erase((p_ROB->dest_tag).begin());
                                        (p_ROB->dest_reg).erase((p_ROB->dest_reg).begin());
                                        (p_ROB->retired).erase((p_ROB->retired).begin());
                                        (p_ROB->ready).erase((p_ROB->ready).begin());
                                        (p_ROB->exception).erase((p_ROB->exception).begin());
                                        (p_ROB->PC).erase((p_ROB->PC).begin());
                                }
                        }
                }
        }
        else if(m_s == 2)
        {
                
               
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        if(((p_schedQ->retired)[i] == 1) && ((p_schedQ->valid)[i] == 1))
                        {
                                // update stats
                                //result_deleted[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                                //retired_inst_count++;
                                
                                retired_inst_number = retired_inst_number_pr;
                                (p_schedQ->valid)[i] = 0;
                                (p_schedQ->FU_assigned)[i] = 0;
                                (p_schedQ->srcreg_ready)[i] = 0;
                                (p_schedQ->completed)[i] = 0;   
                                (p_schedQ->retired)[i] = 0;
                                (p_schedQ->inBQ)[i] = 0;
                                (p_schedQ->inFUQ)[i] = 0;               
                        }
                }
        }
}

/**
 * Subroutine to update scheduling queues via result bus
 * Subroutine to send dispatched instructions to scheduling queue depending upon available locations in SQ and update SQ state
 * Scheduling queue has a size of 2 * total number of FUs
 */
void schedule(s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB)
{
        // all local variables
        uint32_t i, j;
       
        //print_SQ(p_schedQ);

        // update entries in SQ from the CDB      
        for(i = 0; i < (p_schedQ->size); i++)
        {      
                // check if src1 needs any updation
                if(((p_schedQ->src1_ready)[i] == 0) && ((p_schedQ->valid)[i] == 1))
                {
                        for(j = 0; j < m_r; j++)
                        {
                                if(((p_schedQ->src1_tag)[i] == ((p_cdb->cdb_tag)[j])) && ((p_cdb->cdb_busy)[j] == 1))
                                {
                                        (p_schedQ->src1_ready)[i] = 1;
                                       
                                }
                        }
                }
               
                // check if src2 needs any updation
                if(((p_schedQ->src2_ready)[i] == 0) && ((p_schedQ->valid)[i] == 1))
                {
                        for(j = 0; j < m_r; j++)
                        {
                                if(((p_schedQ->src2_tag)[i] == ((p_cdb->cdb_tag)[j])) && ((p_cdb->cdb_busy)[j] == 1))
                                {
                                        (p_schedQ->src2_ready)[i] = 1;
                                }
                        }
                }                              
        }
       
        // update entries in DQ from the CDB      
        for(i = 0; i < (p_dispatchQ->inst_number).size(); i++)
        {      
                // check if src1 needs any updation
                if((p_dispatchQ->src1_ready)[i] == 0)
                {
                        for(j = 0; j < m_r; j++)
                        {
                                if(((p_dispatchQ->src1_tag)[i] == ((p_cdb->cdb_tag)[j])) && ((p_cdb->cdb_busy)[j] == 1))
                                {
                                        (p_dispatchQ->src1_ready)[i] = 1;
                                       
                                }
                        }
                }
               
                // check if src2 needs any updation
                if((p_dispatchQ->src2_ready)[i] == 0)
                {
                        for(j = 0; j < m_r; j++)
                        {
                                if(((p_dispatchQ->src2_tag)[i] == ((p_cdb->cdb_tag)[j])) && ((p_cdb->cdb_busy)[j] == 1))
                                {
                                        (p_dispatchQ->src2_ready)[i] = 1;
                                }
                        }
                }                              
        }                 
        
        // dispatch unit reserves slots in scheduling queue and marks it as SQ
        // once allocated in SQ, delete from DQ
        // check if ROB has space. If yes, put instruction into ROB and SQ
        uint32_t b = (p_dispatchQ->inst_number).size();
        if(b != 0)   // while dispatch queue is not empty
        {
                dispatchQ_size.push_back((p_dispatchQ->inst_number).size());
                //cout<<"DispQ Size: "<<(p_dispatchQ->inst_number).size();
        
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        uint32_t a = (p_dispatchQ->inst_number).size();
                        // if queue is empty, put instruction in it, and dispatch queue has valid instructions in it
                        if(((p_schedQ->valid)[i] == 0) && ( a != 0))
                        {
                                result_dispatched[(p_dispatchQ->inst_number)[0] - 1] = cycle_number;
                                (p_schedQ->valid)[i] = 1;    
                                (p_schedQ->inst_number)[i] = (p_dispatchQ->inst_number)[0];
                                (p_schedQ->FU)[i] = (p_dispatchQ->op_code)[0];
                                (p_schedQ->dest_reg)[i] = (p_dispatchQ->dest_reg)[0];
                                (p_schedQ->dest_tag)[i] = (p_dispatchQ->dest_tag)[0];
                                (p_schedQ->src1_ready)[i] = (p_dispatchQ->src1_ready)[0];
                                (p_schedQ->src2_ready)[i] = (p_dispatchQ->src2_ready)[0];
                                (p_schedQ->src1_tag)[i] = (p_dispatchQ->src1_tag)[0];
                                (p_schedQ->src2_tag)[i] = (p_dispatchQ->src2_tag)[0];
                                (p_schedQ->srcreg_ready)[i] = 0;
                                (p_schedQ->FU_assigned)[i] = 0;  
                                (p_schedQ->completed)[i] = 0;
                                (p_schedQ->retired)[i] = 0;
                                (p_schedQ->exception)[i] = 0;
                                latestinSQ = (p_dispatchQ->inst_number)[0] - 1;
                                //cout<<"\nlatest from schedule: "<<latestinSQ+1;
                                if(m_s == 2)
                                {
                                        if((p_dispatchQ->src_reg_1)[0] != -1)
                                                regfile_hits++;
                                        if((p_dispatchQ->src_reg_2)[0] != -1)
                                                regfile_hits++;
                                }
                                
                                                          
                                if(m_s == 1)
                                {  
                                        ROB_regfile(p_dispatchQ, p_ROB, p_regfile);                         
                                        (p_ROB->dest_tag).push_back((p_dispatchQ->dest_tag)[0]);
                                        (p_ROB->dest_reg).push_back((p_dispatchQ->dest_reg)[0]);
                                        (p_ROB->retired).push_back(0);
                                        (p_ROB->ready).push_back(0);
                                        (p_ROB->exception).push_back(0);
                                        (p_ROB->PC).push_back((p_dispatchQ->inst_number)[0] - 1); 
                                }
                                     
                                // delete instruction from dispatch queue as it is now in scheduling queue    
                                (p_dispatchQ->op_code).erase((p_dispatchQ->op_code).begin());
                               
                                (p_dispatchQ->src_reg_1).erase((p_dispatchQ->src_reg_1).begin());
                                (p_dispatchQ->src1_tag).erase((p_dispatchQ->src1_tag).begin());
                                (p_dispatchQ->src1_ready).erase((p_dispatchQ->src1_ready).begin());
                               
                                (p_dispatchQ->src_reg_2).erase((p_dispatchQ->src_reg_2).begin());
                                (p_dispatchQ->src2_tag).erase((p_dispatchQ->src2_tag).begin());
                                (p_dispatchQ->src2_ready).erase((p_dispatchQ->src2_ready).begin());
                               
                                (p_dispatchQ->dest_reg).erase((p_dispatchQ->dest_reg).begin());
                                (p_dispatchQ->dest_tag).erase((p_dispatchQ->dest_tag).begin());
                               
                                (p_dispatchQ->inst_number).erase((p_dispatchQ->inst_number).begin());
                               
                                                     
                        }    
                }
        }
        //cout<<"\nROB hits: "<<ROB_hits;
        //cout<<"\nRegfile hits: "<<regfile_hits;
        //print_SQ(p_schedQ);
}

/**
 * Subroutine to fire any independent instructions from the scheduling queue
 * Check for availability of FU, and set FU_assigned bit and update EX state
 * EX means instruction is now in the EX unit
 */
void execute(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile)
{
        // all local variables
        uint32_t i;
        int32_t low_tag, low_index;
        bool flag;

        // check for firing and update EX state
        // independent instructions in SQ marked to srcreg ready
        for(i = 0; i < (p_schedQ->size); i++)
        {
                if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 0) && ((p_schedQ->src1_ready)[i] == 1) && ((p_schedQ->src2_ready)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 0) && ((p_schedQ->completed)[i] == 0) && ((p_schedQ->retired)[i] == 0))
                {
                        (p_schedQ->srcreg_ready)[i] = 1;
                       // result_executed[(p_schedQ->inst_number)[i] - 1] = cycle_number;  
                }
        }
       
        // check available FUs and assign them in tag order
        // set FU_assigned bit if FU is available
               
        for(uint32_t k = 0; k < (p_schedQ->size); k++)
        {      
                low_tag = 100000;
                low_index = 100;
                flag = 1;
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 0) && ((p_schedQ->inFUQ)[i] == 0) && ((p_schedQ->src1_ready)[i] == 1) && ((p_schedQ->src2_ready)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->completed)[i] == 0) && ((p_schedQ->retired)[i] == 0))
                        {
                                if((p_schedQ->dest_tag)[i] < low_tag)
                                {
                                        low_tag = (p_schedQ->dest_tag)[i];
                                        low_index = i;
                                        flag = 0;
                                }
                        }
                }
                if(flag == 0)   // meaning i found an instruction with lowest tag ready to be given an FU
                {
                        (p_schedQ->FUQ_no).push_back(low_index);
                        //(p_schedQ->FUQ_cycle).push_back(cycle_number);
                        (p_schedQ->inFUQ)[low_index] = 1;                                             
                }
         }
         //cout<<"\ncycle:"<<cycle_number;
         //cout<<"\nFUQ after assigning";
         //print_FUQ(p_schedQ);       
         
         uint32_t j;
         int32_t temp1, temp2;
         // arrange the entire FUQ in tag order now
         if((p_schedQ->FUQ_no).size() != 0)
         {
                for(i = 0; i < (p_schedQ->FUQ_no).size(); i++)
                {
                        temp1 = (p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                        for(j = i; j < (p_schedQ->FUQ_no).size(); j++)
                        {
                                if((p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[j]] < temp1)
                                {
                                        temp1 = (p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[j]];
                                        temp2 = (p_schedQ->FUQ_no)[j];
                                        (p_schedQ->FUQ_no)[j] = (p_schedQ->FUQ_no)[i];
                                        (p_schedQ->FUQ_no)[i] = temp2;
                                }       
                        }
                }
         }
         //cout<<"\nFUQ after arranging in ascending";
         //print_FUQ(p_schedQ);       
           
               
         // now match it with an FU type and allocate an available FU of that type
         if((p_schedQ->FUQ_no).size() != 0)
         {       
                //int32_t x = (p_schedQ->FUQ_cycle)[0];
                for(i = 0; i < (p_schedQ->FUQ_no).size(); i++)
                {     
                        FU_assign(i, p_schedQ, p_scoreboard);
                       
                }                       
        }
       
        std::vector<int> eraseindex;
        eraseindex.clear();
        // now delete instructions from FUQ for instructions that have been assigned FU and mark their inFUQ bits false
        if((p_schedQ->FUQ_no).size() != 0)
        {
                for(i = 0; i < (p_schedQ->FUQ_no).size(); i++)
                {
                        if(((p_schedQ->valid)[(p_schedQ->FUQ_no)[i]] == 1) && ((p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] == 1) && ((p_schedQ->inFUQ)[(p_schedQ->FUQ_no)[i]] == 1) && ((p_schedQ->srcreg_ready)[(p_schedQ->FUQ_no)[i]] == 1))
                        {
                                //(p_schedQ->FUQ_no).erase((p_schedQ->FUQ_no).begin() + i);
                                eraseindex.push_back(i);
                                (p_schedQ->inFUQ)[(p_schedQ->FUQ_no)[i]] = 0;      
                        }
                }
        }
       
        if(eraseindex.size() != 0)
        {
                //cout<<"\nFU Erase";
                for(i = eraseindex.size() - 1; i > 0; i--)
                {
                        (p_schedQ->FUQ_no).erase((p_schedQ->FUQ_no).begin() + eraseindex[i]);
                        //(p_schedQ->FUQ_cycle).erase((p_schedQ->FUQ_cycle).begin() + eraseindex[i]);
                        //cout<<"\t"<<eraseindex[i];
                }
                (p_schedQ->FUQ_no).erase((p_schedQ->FUQ_no).begin() + eraseindex[0]);
                //(p_schedQ->FUQ_cycle).erase((p_schedQ->FUQ_cycle).begin() + eraseindex[0]);
        }
        //cout<<"\nFUQ after erasing";
        //print_FUQ(p_schedQ);       
       
        //print_FU(p_scoreboard);   
}

/**
 * Subroutine to assign the given instruction any available FU of that type
 */
void FU_assign(int32_t index, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard)
{
        int32_t i = index;
        uint32_t j;
        if((p_schedQ->FU)[(p_schedQ->FUQ_no)[i]] == 0)
        {
                // search for available FU0
                for(j = 0; j < m_k0; j++)
                {
                        if(((p_scoreboard->FU_k0)[j] == 0) && ((p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] == 0))
                        {
                                (p_scoreboard->FU_k0)[j] = 1;
                                (p_scoreboard->tag_k0)[j] = (p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                (p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] = 1;
                                //cout<<"\n"<<(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]]<<"\t"<<(p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                result_scheduled[(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]] - 1] = cycle_number;
                                fired_inst_count++;
                                
                        }
                }      
        }
        else if(((p_schedQ->FU)[(p_schedQ->FUQ_no)[i]] == 1) || ((p_schedQ->FU)[(p_schedQ->FUQ_no)[i]] == -1))
        {
                // search for available FU1
                for(j = 0; j < m_k1; j++)
                {
                        if(((p_scoreboard->FU_k1)[j] == 0) && ((p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] == 0))
                        {
                                (p_scoreboard->FU_k1)[j] = 1;
                                (p_scoreboard->tag_k1)[j] = (p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                (p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] = 1;
                                //cout<<"\n"<<(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]]<<"\t"<<(p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                result_scheduled[(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]] - 1] = cycle_number;
                                fired_inst_count++;
                                
                        }
                }      
        } 
        else if((p_schedQ->FU)[(p_schedQ->FUQ_no)[i]] == 2)
        {
                // search for available FU2
                for(j = 0; j < m_k2; j++)
                {
                        if(((p_scoreboard->FU_k2)[j] == 0) && ((p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] == 0))
                        {
                                (p_scoreboard->FU_k2)[j] = 1;
                                (p_scoreboard->tag_k2)[j] = (p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                (p_schedQ->FU_assigned)[(p_schedQ->FUQ_no)[i]] = 1;
                                //cout<<"\n"<<(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]]<<"\t"<<(p_schedQ->dest_tag)[(p_schedQ->FUQ_no)[i]];
                                result_scheduled[(p_schedQ->inst_number)[(p_schedQ->FUQ_no)[i]] - 1] = cycle_number;
                                fired_inst_count++;
                                
                        }
                }      
        }  
}


/**
 * Subroutine to retire completed instructions and free FU
 * Broadcast executed results on CDB depending upon availability of CDB and mark them completed and update SU stats
 * To update register file via result bus
 */
void retire(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile, s_ROB* p_ROB)
{
        // all local variables used
        uint32_t i, j, k;
        int32_t low_tag, low_index, temp_index;
        bool flag;
        
        // handle exceptions or mark completed instructions as retired
        if(m_s == 1)
        {
                if((p_ROB->exception).size() != 0)
                {
                        for(j = 0; j < (p_ROB->PC).size(); j++)
                        {
                                for(i = 0; i < (p_schedQ->size); i++)
                                {
                                        if(((p_ROB->PC)[j] + 1) == (p_schedQ->inst_number)[i])
                                        {
                                                if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 1) && ((p_schedQ->completed)[i] == 1) && ((p_schedQ->retired)[i] == 0) && ((p_ROB->ready)[j] == 1))  
                                                {
                                                        (p_schedQ->retired)[i] = 1;
                                                        (p_ROB->retired)[j] = 1;
                                                        (p_fetch->retired)[(p_schedQ->inst_number)[i] - 1] = 1;
                                                        result_deleted[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                                                        
                                                        if((p_ROB->exception)[j] != 1)
                                                        {
                                                                retired_inst_count++;
                                                                
                                                                retired_inst_number = (p_schedQ->inst_number)[i];
                                                        }
                                                        
                                                        if((p_ROB->exception)[j] == 1)
                                                        {
                                                                // handle exception
                                                                p_ROB->exceptionBit = 1;
                                                                (p_ROB->exception)[j] == 0;
                                                                // flush ROB, DQ, SQ, scoreboard, FUQ, BQ, RF
                                                                (p_dispatchQ->op_code).clear();
                                                                (p_dispatchQ->src_reg_1).clear();
                                                                (p_dispatchQ->src1_tag).clear();
                                                                (p_dispatchQ->src1_ready).clear();
                                                                (p_dispatchQ->src_reg_2).clear();
                                                                (p_dispatchQ->src2_tag).clear();
                                                                (p_dispatchQ->src2_ready).clear();
                                                                (p_dispatchQ->dest_reg).clear();
                                                                (p_dispatchQ->dest_tag).clear();
                                                                (p_dispatchQ->inst_number).clear();
                                                                (p_dispatchQ->empty) = 1; 
                                                
                                                                for(i = 0; i < (p_schedQ->size); i++)
                                                                {
                                                                        (p_schedQ->valid)[i] = 0;     
                                                                        (p_schedQ->completed)[i] = 0;    
                                                                        (p_schedQ->retired)[i] = 0;    
                                                                        (p_schedQ->inBQ)[i] = 0;
                                                                        (p_schedQ->inFUQ)[i] = 0;
                                                                }
                                                                 
                                                                (p_schedQ->broadcastQ_no).clear();        
                                                                (p_schedQ->FUQ_no).clear(); 

                                                                (p_scoreboard->FU_k0).clear();
                                                                (p_scoreboard->FU_k1).clear();
                                                                (p_scoreboard->FU_k2).clear();                        
                                                                (p_scoreboard->FU_k0).assign(m_k0, 0);
                                                                (p_scoreboard->FU_k1).assign(m_k1, 0);
                                                                (p_scoreboard->FU_k2).assign(m_k2, 0);

                                                                for(j = 0; j < m_r; j++)
                                                                {
                                                                      (p_cdb->cdb_busy)[j] = 0;
                                                                }

                                                                (p_ROB->dest_tag).clear();
                                                                (p_ROB->dest_reg).clear();
                                                                (p_ROB->ready).clear();        
                                                                (p_ROB->exception).clear();    
                                                                (p_ROB->PC).clear();        
                                                                (p_ROB->retired).clear(); 
                                                                
                                                                for(i = 0; i < 128; i++)
                                                                {
                                                                        (p_regfile->reg_ready)[i] = 1;
                                                                        (p_regfile->reg_tag)[i] = 100000 + i;
                                                                } 
                                                        }
                                                }                              
                                        }    
                                }
                                if((p_ROB->retired)[j] != 1)
                                        break;                                   
                        }
                }
        }
        else if (m_s == 2)
        {
                flag = 0;
                low_tag = 200000;
                low_index = 100;
                
                std::vector<uint32_t> SQindex;
                SQindex.clear();
                // arrange SQ positions in ascending order of tags
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 1) && ((p_schedQ->completed)[i] == 1) && ((p_schedQ->retired)[i] == 0))
                                SQindex.push_back(i);
                }
                /*
                // print SQ for debugging
                if(SQindex.size() != 0)
                {
                        cout<<"\nSQindex before arranging: ";
                        for(j = 0; j < SQindex.size(); j++)
                        { 
                                cout<<"\t"<<((p_schedQ->inst_number)[SQindex[j]]);
                        }
                }*/
                // arrange in ascending order
                if(SQindex.size() != 0)
                {
                        for(j = 0; j < SQindex.size(); j++)
                        { 
                                low_tag = (p_schedQ->dest_tag)[SQindex[j]];
                                low_index = SQindex[j];
                                for(i = j; i < SQindex.size(); i++)
                                {
                                        if((p_schedQ->dest_tag)[SQindex[i]] < low_tag)
                                        {
                                               low_tag = (p_schedQ->dest_tag)[SQindex[i]]; 
                                               temp_index = SQindex[i];
                                               SQindex[j] = SQindex[i];
                                               SQindex[i] = low_index;
                                               low_index = temp_index;
                                               
                                       }                   
                                }
                        }       
                }
                
                /*
                // print SQ for debugging
                if(SQindex.size() != 0)
                {
                        cout<<"\nSQindex: ";
                        for(j = 0; j < SQindex.size(); j++)
                        { 
                                cout<<"\t"<<(p_schedQ->inst_number)[SQindex[j]]<<"("<<(p_schedQ->exception)[SQindex[j]]<<")" ;
                        }
                }
                */
                // do operations                
                if(SQindex.size() != 0)
                {
                        for(i = 0; i < SQindex.size(); i++)
                        {
                                if((p_schedQ->exception)[SQindex[i]] == 1)
                                {
                                        // handle exception
                                        //cout<<"\nException Thrown";
                                        exceptionBit = 1;
                                        
                                        // flush DQ, SQ, scoreboard, FUQ, BQ
                                        
                                        (p_dispatchQ->op_code).clear();
                                        (p_dispatchQ->src_reg_1).clear();
                                        (p_dispatchQ->src1_tag).clear();
                                        (p_dispatchQ->src1_ready).clear();
                                        (p_dispatchQ->src_reg_2).clear();
                                        (p_dispatchQ->src2_tag).clear();
                                        (p_dispatchQ->src2_ready).clear();
                                        (p_dispatchQ->dest_reg).clear();
                                        (p_dispatchQ->dest_tag).clear();
                                        (p_dispatchQ->inst_number).clear();
                                        (p_dispatchQ->empty) = 1; 
                        
                                        for(j = 0; j < (p_schedQ->size); j++)
                                        {
                                                (p_schedQ->valid)[j] = 0;  
                                                (p_schedQ->FU_assigned)[j] = 0; 
                                                (p_schedQ->srcreg_ready)[j] = 0;  
                                                (p_schedQ->completed)[j] = 0;    
                                                (p_schedQ->retired)[j] = 0;    
                                                (p_schedQ->inBQ)[j] = 0;
                                                (p_schedQ->inFUQ)[j] = 0;
                                                (p_schedQ->exception)[j] = 0;
                                        }
                                         
                                        (p_schedQ->broadcastQ_no).clear();        
                                        (p_schedQ->FUQ_no).clear(); 

                                        (p_scoreboard->FU_k0).clear();
                                        (p_scoreboard->FU_k1).clear();
                                        (p_scoreboard->FU_k2).clear();                        
                                        (p_scoreboard->FU_k0).assign(m_k0, 0);
                                        (p_scoreboard->FU_k1).assign(m_k1, 0);
                                        (p_scoreboard->FU_k2).assign(m_k2, 0);

                                        for(k = 0; k < m_r; k++)
                                        {
                                              (p_cdb->cdb_busy)[k] = 0;
                                        }
                                        
                                        // copy B2 to B1 and messy
                                        for(k = 0; k < 128; k++)
                                        {
                                                (p_regfile->reg_tag)[k] = (p_regfile->B2_reg_tag)[k];
                                                (p_regfile->B1_reg_tag)[k] = (p_regfile->B2_reg_tag)[k];
                                                (p_regfile->reg_ready)[k] = (p_regfile->B2_reg_ready)[k];
                                                (p_regfile->B1_reg_ready)[k] = (p_regfile->B2_reg_ready)[k];   
                                        }
                                }
                                else
                                {
                                        // mark instructions as retired
                                        //retired_inst_count++;
                                        
                                        retired_inst_number_pr = (p_schedQ->inst_number)[SQindex[i]];
                                        (p_schedQ->retired)[SQindex[i]] = 1;
                                        (p_fetch->retired)[(p_schedQ->inst_number)[SQindex[i]] - 1] = 1;                                
                                        result_deleted[(p_schedQ->inst_number)[SQindex[i]] - 1] = cycle_number;
                                        
                                        // when all instructions between IB1 and IB2 have retired, copy B1 to B2 and make new IB1
                                        int32_t det = 0;
                                        int32_t l;
                                        for(l = IB2; l <= IB1; l++)
                                        {
                                                if((p_fetch->retired)[l] == 1)
                                                        det++;
                                                else
                                                        det = 0;
                                        }
                                        if(det == (IB1-IB2+1))
                                        {
                                                //cout<<"\nBackup "<<IB0+1<<" to "<<IB1+1;
                                                IB2 = IB1 + 1;
                                                IB0 = IB1 + 1;
                                                IB1 = latestinSQ;
                                                //cout<<"\nNew IB1: "<<IB1+1;
                                                //cout<<"\nNew IB2: "<<IB2+1;
                                                for(j = 0; j < 128; j++)
                                                {
                                                        (p_regfile->B2_reg_tag)[j] = (p_regfile->B1_reg_tag)[j];
                                                
                                                }
                                                backup_count++;
                                        }
                                }
                        }
                }
        }
        // broadcast on CDB
        // first invalidate all buses
        for(j = 0; j < m_r; j++)
        {
              (p_cdb->cdb_busy)[j] = 0;
        }
           
        flag = 0;
        low_tag = 200000;
        low_index = 100;
        // check for instructions that can be broadcasted, if yes, arrange them in tag order in the broadcastQ and mark their inBQ bits
        for(k = 0; k < (p_schedQ->size); k++)
        {
                flag = 0;
                low_tag = 200000;
                low_index = 100;
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 1) && ((p_schedQ->completed)[i] == 0) && ((p_schedQ->inBQ)[i] == 0) && ((p_schedQ->retired)[i] == 0))  
                        {    
                                // update stats
                                result_executed[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                                
                                if(m_s == 1)
                                {
                                        if((p_ROB->PC).size() != 0)
                                        {
                                                for(j = 0; j < (p_ROB->PC).size(); j++)
                                                {
                                                        if(((p_schedQ->inst_number)[i] - 1) == (p_ROB->PC)[j])
                                                        {
                                                                (p_ROB->ready)[j] = 1;
                                                                if((((p_ROB->PC)[j] + 1) % m_e) == 0)
                                                                {
                                                                        bool handled = 0;
                                                                        uint32_t l;
                                                                        if((exception_PC).size() != 0)
                                                                        {
                                                                                for(l = 0; l < (exception_PC).size(); l++)
                                                                                {
                                                                                        if((exception_PC)[l] == (p_ROB->PC)[j])
                                                                                        {
                                                                                                handled = 1;
                                                                                        }
                                                                                }
                                                                        }
                                                                        if(handled == 0)
                                                                        {
                                                                                return_pc = (p_ROB->PC)[j];
                                                                                //cout<<"\nRPC: "<<return_pc;
                                                                                (p_ROB->exception)[j] = 1;
                                                                                exception_PC.push_back(return_pc);
                                                                        }
                                                                }
                                                        }
                                                }
                                        }  
                                }
                                else if(m_s == 2)
                                {
                                        if(((((p_schedQ->inst_number)[i]) % m_e) == 0) && ((p_schedQ->exception)[i] == 0))
                                        {
                                                //cout<<"\nException instruction:"<<(p_schedQ->inst_number)[i];
                                                bool handled = 0;
                                                uint32_t l;
                                                if((exception_PC).size() != 0)
                                                {
                                                        for(l = 0; l < (exception_PC).size(); l++)
                                                        {
                                                                if((exception_PC)[l] == ((p_schedQ->inst_number)[i] - 1))
                                                                {
                                                                        
                                                                        handled = 1;
                                                                        (p_schedQ->exception)[i] = 0;
                                                                }
                                                        }
                                                }
                                                if(handled == 0)
                                                {
                                                        (p_schedQ->exception)[i] = 1;
                                                        //cout<<"\nException bit set of "<<(p_schedQ->inst_number)[i];
                                                        exception_PC.push_back((p_schedQ->inst_number)[i] - 1);
                                                        //print_exceptionPC();
                                                }
                                                //print_SQ(p_schedQ);
                                        }
                                
                                }
                                
                                if((p_schedQ->dest_tag)[i] < low_tag)
                                {
                                        low_tag = (p_schedQ->dest_tag)[i];
                                        low_index = i;
                                        flag = 1;
                                }                                  
                        }
                }
                if(flag == 1)
                {
                        (p_schedQ->broadcastQ_no).push_back(low_index);
                        (p_schedQ->inBQ)[low_index] = 1;
                       
                }     
        } 
        //print_BQ(p_schedQ);
        // now check for available CDB and broadcast them as per broadcast queue
        if((p_schedQ->broadcastQ_no).size() != 0)
        {
                for(j = 0; j < m_r; j++)
                {
                        for(i = 0; i < (p_schedQ->broadcastQ_no).size(); i++)
                        {     
                                if(((p_schedQ->valid)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->srcreg_ready)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->FU_assigned)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->inBQ)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->completed)[(p_schedQ->broadcastQ_no)[i]] == 0) && ((p_schedQ->retired)[(p_schedQ->broadcastQ_no)[i]] == 0) && ((p_cdb->cdb_busy)[j] == 0))
                                {
                                        (p_schedQ->completed)[(p_schedQ->broadcastQ_no)[i]] = 1;   // done putting it on cdb
                                        (p_cdb->cdb_tag)[j] = (p_schedQ->dest_tag)[(p_schedQ->broadcastQ_no)[i]];
                                        (p_cdb->cdb_destreg)[j] = (p_schedQ->dest_reg)[(p_schedQ->broadcastQ_no)[i]];
                                        (p_cdb->cdb_busy)[j] = 1;   
                                       
                                        // update stats
                                        
                                        //result_broadcasted[(p_schedQ->inst_number)[(p_schedQ->broadcastQ_no)[i]] - 1] = cycle_number;
                                }
                        }                 
                }
        }

        std::vector<int> eraseindex;
        eraseindex.clear();
        // delete entry from broadcastQ of broadcasted instruction and reset inBQ bit
        if((p_schedQ->broadcastQ_no).size() != 0)
        {
                for(i = 0; i < (p_schedQ->broadcastQ_no).size(); i++)
                {
                        if(((p_schedQ->valid)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->srcreg_ready)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->FU_assigned)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->inBQ)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->completed)[(p_schedQ->broadcastQ_no)[i]] == 1) && ((p_schedQ->retired)[(p_schedQ->broadcastQ_no)[i]] == 0))
                        {
                                //(p_schedQ->broadcastQ_no).erase((p_schedQ->broadcastQ_no).begin() + i);
                                eraseindex.push_back(i);
                                (p_schedQ->inBQ)[(p_schedQ->broadcastQ_no)[i]] = 0;  
                        }
                       
                }    
        }

        if(eraseindex.size() != 0)
        {
                //cout<<"\nBQ Erase";
                for(i = eraseindex.size() - 1; i > 0; i--)
                {
                        (p_schedQ->broadcastQ_no).erase((p_schedQ->broadcastQ_no).begin() + eraseindex[i]);
                        //cout<<"\t"<<eraseindex[i];
                }
                (p_schedQ->broadcastQ_no).erase((p_schedQ->broadcastQ_no).begin() + eraseindex[0]);
        }       
       
        // release FU
        for(i = 0; i < (p_schedQ->size); i++)
        {
                if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 1) && ((p_schedQ->completed)[i] == 1) && ((p_schedQ->retired)[i] == 0))  
                {                     
                       // free FU of that instruction
                       if((p_schedQ->FU)[i] == 0)
                        {
                                for(k = 0; k < m_k0; k++)
                                {
                                        if(((p_scoreboard->FU_k0)[k] == 1) && ((p_scoreboard->tag_k0)[k] == (p_schedQ->dest_tag)[i]))
                                        {
                                                (p_scoreboard->FU_k0)[k] = 0;
                                        }
                                }
                        }
                        else if(((p_schedQ->FU)[i] == 1) || ((p_schedQ->FU)[i] == -1))
                        {
                                 for(k = 0; k < m_k1; k++)
                                {
                                        if(((p_scoreboard->FU_k1)[k] == 1) && ((p_scoreboard->tag_k1)[k] == (p_schedQ->dest_tag)[i]))
                                        {
                                                (p_scoreboard->FU_k1)[k] = 0;
                                        }
                                }                              
                        }
                        else if((p_schedQ->FU)[i] == 2)
                        {
                                 for(k = 0; k < m_k2; k++)
                                {
                                        if(((p_scoreboard->FU_k2)[k] == 1) && ((p_scoreboard->tag_k2)[k] == (p_schedQ->dest_tag)[i]))
                                        {
                                                (p_scoreboard->FU_k2)[k] = 0;
                                        }
                                }                              
                        }                                  
                }    
        }

        // register file updated, for whose destination register exists and tag matches with broadcasting cdb tag
        // update messy file and if any, B1 reg file
        if(m_s == 2)
        {
                for(i = 0; i < m_r; i++)
                {
                        if(((p_cdb->cdb_busy)[i] == 1) && ((p_cdb->cdb_destreg)[i] != -1) && ((p_cdb->cdb_tag)[i] == (p_regfile->reg_tag)[(p_cdb->cdb_destreg)[i]]))
                        {
                                (p_regfile->reg_ready)[(p_cdb->cdb_destreg)[i]] = 1;
                        }
                        // B1 updated considering IB1
                        if(((p_cdb->cdb_busy)[i] == 1) && ((p_cdb->cdb_destreg)[i] != -1) && ((p_cdb->cdb_tag)[i] == (p_regfile->B1_reg_tag)[(p_cdb->cdb_destreg)[i]]))
                        {
                                if((p_cdb->cdb_tag)[i] <= IB1)
                                        (p_regfile->B1_reg_ready)[(p_cdb->cdb_destreg)[i]] = 1;
                        }

                }
        }
}



/*
* Subroutine to calculte ROB hits and regfile hits
*/

void ROB_regfile(s_dispatchQ* p_dispatchQ, s_ROB* p_ROB, s_regfile* p_regfile)
{
        uint32_t i;
        bool flag1 = 0; // =1: found in ROB for src1
        bool flag2 = 0; // =1: found in ROB for src2
        
        // first search in ROB and then reg file for src1 and src2 tags and ready bits
        if((p_dispatchQ->src_reg_1)[0] != -1)
        {
                if((p_ROB->dest_reg).size() != 0)
                {
                        for(i = (p_ROB->dest_reg).size(); i > 0; i--)
                        {
                                if((((p_ROB->dest_reg)[i - 1]) == ((p_dispatchQ->src_reg_1)[0])) && (flag1 == 0))
                                {
                                        ROB_hits++;
                                        flag1 = 1; 
                                }
                        }
                }
                if(flag1 == 0)
                {
                        regfile_hits++;
                }          
        }

        if((p_dispatchQ->src_reg_2)[0] != -1)
        {
                if((p_ROB->dest_reg).size() != 0)
                {
                        for(i = (p_ROB->dest_reg).size(); i > 0; i--)
                        {
                                if((((p_ROB->dest_reg)[i - 1]) == ((p_dispatchQ->src_reg_2)[0])) && (flag2 == 0))
                                {
                                        ROB_hits++;
                                        flag2 = 1; 
                                }
                        }
                }
                if(flag2 == 0)
                {
                        regfile_hits++;
                }          
        }
        
}
