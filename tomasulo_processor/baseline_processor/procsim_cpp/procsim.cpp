
#include "procsim.hpp"

using namespace std;

// some global variables
uint64_t m_r, m_f, m_k0, m_k1, m_k2;     // copying system parameters 
uint64_t cycle_number = 1;     // cycle number
uint32_t g_inst_number = 1;       // instruction number = line number of trace file
uint32_t retired_inst_number = 0;
uint32_t g_tag = 0;             // generating tags for instructions sequentially
std::vector<uint32_t> dispatchQ_size;

// all local structures
struct s_fetch {
    std::vector<uint32_t> m_instruction_address;
        std::vector<int32_t> m_op_code;
        std::vector<int32_t> m_src_reg_0;
        std::vector<int32_t> m_src_reg_1;
        std::vector<int32_t> m_dest_reg;
        std::vector<int32_t> inst_number;
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
        //std::vector<int32_t> FUQ_cycle;         // stores cycle number of instructions when FU was requested     
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

// register file
struct s_regfile {
        int32_t reg_ready[128];
        int32_t reg_tag[128];
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
void init(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile);
void fetch(proc_inst_t* p_inst, s_fetch* p_fetch);
void rm_retired(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard);
void dispatch(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_regfile* p_regfile);
void schedule(s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile);
void execute(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile);
void FU_assign(int32_t index, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard);
void retire(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile);


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
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f)
{
        // all local variables used here
        int32_t i;
       
    // initializing variables being used
    m_r = r;     // number of result buses
    m_f = f;    // number of instructions fetched
    m_k0 = k0;    // number of k0 FUs
    m_k1 = k1;    // number of k1 FUs
    m_k2 = k2;     // number of k2 FUs
   
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
   
    init(&m_fetch, &m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);    // initializing any parameters to be used

    // this for loop has to be replaced with retire instruction condition
    while(retired_inst_number != 100000)
    {
           
            retire(&m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);
            execute(&m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);
            schedule(&m_dispatchQ, &m_schedQ, &m_scoreboard, &m_cdb, &m_regfile);
            //dispatch(&m_fetch, &m_dispatchQ, &m_regfile);
            rm_retired(&m_schedQ, &m_scoreboard);
            fetch(p_inst, &m_fetch);
            dispatch(&m_fetch, &m_dispatchQ, &m_regfile);
       
        //cout<<"\nCYCLE "<<cycle_number;
        //print_DQ(&m_dispatchQ);
       
        //print_SQ(&m_schedQ);
        //print_FU(&m_scoreboard);
        //print_CDB(&m_cdb);
        //print_regfile(&m_regfile);
        //print_results();
        //print_FUQ(&m_schedQ);
        //print_BQ(&m_schedQ);
        cycle_number++;

    }
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
        p_stats->avg_inst_fired = (double) (g_inst_number - 1) / (double) (cycle_number - 1);
        
        // average number of instructions retired per cycle
        p_stats->avg_inst_retired = (double) (retired_inst_number) / (double) (cycle_number - 1);
         
        // total run time
        p_stats->cycle_count = cycle_number - 1;
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
        cout<<"\nINST\tDEST\tSRC1_TAG\tSRC1_READY\tSRC2_TAG\tSRC2_READY\tSRCREG_READY\tFU_ASG\tCOMP\tRET";
        for(i = 0; i < (p_schedQ->size); i++)
        {
                cout<<"\n"<<(p_schedQ->inst_number)[i]<<"("<<(p_schedQ->valid)[i]<<")\t"<<(p_schedQ->dest_tag)[i]<<"\t"<<(p_schedQ->src1_tag)[i]<<"\t\t"<<(p_schedQ->src1_ready)[i]<<"\t\t"<<(p_schedQ->src2_tag)[i]<<"\t\t"<<(p_schedQ->src2_ready)[i]<<"\t\t"<<(p_schedQ->srcreg_ready)[i]<<"\t\t"<<(p_schedQ->FU_assigned)[i]<<"\t"<<(p_schedQ->completed)[i]<<"\t"<<(p_schedQ->retired)[i];
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

/**
 * Subroutine to initialize all counters and parameters
 */
void init(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile)
{
        uint32_t i;
      
        p_fetch->empty = 1;
       
        p_dispatchQ->empty = 1;
 
        p_schedQ->size = 2 * (m_k0 + m_k1 + m_k2);
       
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
                (p_regfile->reg_tag)[i] = i;
        } 
}

/**
 * Subroutine to fetch F instructions per cycle.
 * Fetched instructions are stored in fetch buffer of size m_f
 * update FETCH stats
 */
void fetch(proc_inst_t* p_inst, s_fetch* p_fetch)
{
    // variables used in fetch()
    uint32_t i;
   
    // clear all vectors
    (p_fetch->m_instruction_address).clear();
    (p_fetch->m_op_code).clear();
    (p_fetch->m_src_reg_0).clear();
    (p_fetch->m_src_reg_1).clear();
    (p_fetch->m_dest_reg).clear();
    (p_fetch->inst_number).clear();
    p_fetch->empty = 1;  
       
    // fetching f instructions in one cycle
    for (i = 0; i < m_f; i++)
    {
        if (read_instruction(p_inst) == true)
        {
                (p_fetch->m_instruction_address).push_back(p_inst->instruction_address);           
                (p_fetch->m_op_code).push_back(p_inst->op_code);
                (p_fetch->m_src_reg_0).push_back(p_inst->src_reg[0]);
                (p_fetch->m_src_reg_1).push_back(p_inst->src_reg[1]);
                (p_fetch->m_dest_reg).push_back(p_inst->dest_reg);
                (p_fetch->inst_number).push_back(g_inst_number);

                // update stats
                p_fetch->empty = 0;     // not empty anymore
                result_fetched[g_inst_number - 1] = cycle_number;                  
                g_inst_number++;      
        }   
    }      
}

/**
 * Subroutine to delete retired instructions from the scheduling queue
 */
void rm_retired(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard)
{
        // all local variables
        uint32_t i;
       
        for(i = 0; i < (p_schedQ->size); i++)
        {
                if(((p_schedQ->retired)[i] == 1) && ((p_schedQ->valid)[i] == 1))
                {
                        // update stats
                        result_deleted[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                        retired_inst_number++;
                        
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

/**
 * Subroutine to put newly fetched instructions into the dispatch queue
 * Read from the register file and update src1 and src2 states
 * Update regfile with dest reg of instructions stats
 * Update DISP state
 */
void dispatch(s_fetch* p_fetch, s_dispatchQ* p_dispatchQ, s_regfile* p_regfile)
{
    // variables used in dispatch()
    uint32_t i;
       
        // put in dispatch queue only if fetch is not empty
        if((p_fetch->empty) == 0)
        {       
            // fetching f instructions in one cycle
            for (i = 0; i < m_f; i++)
            {       
                (p_dispatchQ->op_code).push_back((p_fetch->m_op_code)[i]);
                (p_dispatchQ->src_reg_1).push_back((p_fetch->m_src_reg_0)[i]);
                (p_dispatchQ->src_reg_2).push_back((p_fetch->m_src_reg_1)[i]);
                (p_dispatchQ->dest_reg).push_back((p_fetch->m_dest_reg)[i]);
                        (p_dispatchQ->inst_number).push_back((p_fetch->inst_number)[i]);
                        (p_dispatchQ->dest_tag).push_back(g_tag);
                       
                        // search in reg file for src1 and src2 tags and ready bits
                        // for src1
                        if((p_fetch->m_src_reg_0)[i] == -1)
                        {
                                (p_dispatchQ->src1_ready).push_back(1);
                                (p_dispatchQ->src1_tag).push_back(-1);
                        }
                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[i]] == 1)
                        {
                                (p_dispatchQ->src1_ready).push_back(1);
                                (p_dispatchQ->src1_tag).push_back(-1);
                        }
                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_0)[i]] == 0)
                        {
                                (p_dispatchQ->src1_ready).push_back(0);
                                (p_dispatchQ->src1_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_0)[i]]);  
                        }
                       
                        // for src2
                        if((p_fetch->m_src_reg_1)[i] == -1)
                        {
                                (p_dispatchQ->src2_ready).push_back(1);
                                (p_dispatchQ->src2_tag).push_back(-1);
                        }
                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[i]] == 1)
                        {
                                (p_dispatchQ->src2_ready).push_back(1);
                                (p_dispatchQ->src2_tag).push_back(-1);
                        }
                        else if((p_regfile->reg_ready)[(p_fetch->m_src_reg_1)[i]] == 0)
                        {
                                (p_dispatchQ->src2_ready).push_back(0);
                                (p_dispatchQ->src2_tag).push_back((p_regfile->reg_tag)[(p_fetch->m_src_reg_1)[i]]);
                        }

                        // reserve dest reg file
                        if((p_fetch->m_dest_reg)[i] != -1)
                        {
                                (p_regfile->reg_ready)[(p_fetch->m_dest_reg)[i]] = 0;
                                (p_regfile->reg_tag)[(p_fetch->m_dest_reg)[i]] = g_tag;
                        }   
                       
                        // update stats                   
                        //result_dispatched[(p_fetch->inst_number)[i] - 1] = cycle_number;
                        g_tag++;
                                                           
            }   
            p_dispatchQ->empty = 0;     // not empty anymore 
        }
        dispatchQ_size.push_back((p_dispatchQ->inst_number).size());      
}

/**
 * Subroutine to update scheduling queues via result bus
 * Subroutine to send dispatched instructions to scheduling queue depending upon available locations in SQ and update SQ state
 * Scheduling queue has a size of 2 * total number of FUs
 */
void schedule(s_dispatchQ* p_dispatchQ, s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile)
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
        if((p_dispatchQ->empty) == 0)   // while dispatch queue is not empty
        {
                for(i = 0; i < (p_schedQ->size); i++)
                {
                        // if queue is empty, put instruction in it, and dispatch queue has valid instructions in it
                        if(((p_schedQ->valid)[i] == 0) && ((p_dispatchQ->inst_number).size() != 0)) 
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
                                         
                                // update stats
                                //result_scheduled[(p_schedQ->inst_number)[i] - 1] = cycle_number;
                         }             
                 }                                            
         }
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
                        }
                }      
        }
       
        
}


/**
 * Subroutine to retire completed instructions and free FU
 * Broadcast executed results on CDB depending upon availability of CDB and mark them completed and update SU stats
 * To update register file via result bus
 */
void retire(s_schedQ* p_schedQ, s_scoreboard* p_scoreboard, s_cdb* p_cdb, s_regfile* p_regfile)
{

        // all local variables used
        uint32_t i, j, k;
        int32_t low_tag, low_index;
        bool flag;

        // mark completed instructions as retired
        for(i = 0; i < (p_schedQ->size); i++)
        {
                if(((p_schedQ->valid)[i] == 1) && ((p_schedQ->srcreg_ready)[i] == 1) && ((p_schedQ->FU_assigned)[i] == 1) && ((p_schedQ->completed)[i] == 1) && ((p_schedQ->retired)[i] == 0))  
                {
                        (p_schedQ->retired)[i] = 1;                              
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
        for(i = 0; i < m_r; i++)
        {
                if(((p_cdb->cdb_busy)[i] == 1) && ((p_cdb->cdb_destreg)[i] != -1) && ((p_cdb->cdb_tag)[i] == (p_regfile->reg_tag)[(p_cdb->cdb_destreg)[i]]))
                {
                        (p_regfile->reg_ready)[(p_cdb->cdb_destreg)[i]] = 1;
                }
        }
       
       
}


