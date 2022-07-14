#include "MOSI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOSI_protocol::MOSI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
        this->state = MOSI_CACHE_I;
}

MOSI_protocol::~MOSI_protocol ()
{    
}

void MOSI_protocol::dump (void)
{
    const char *block_states[9] = {"X","I","IM","IS","S","SM","O","OM","M"};
    fprintf (stderr, "MOSI_protocol - state: %s\n", block_states[state]);
}

void MOSI_protocol::process_cache_request (Mreq *request)
{
   switch (state) {
    case MOSI_CACHE_I:  
        //fprintf (stderr, "\nState I\n");
        do_cache_I (request); 
        break;
    case MOSI_CACHE_IM: 
        do_cache_IM (request); 
        break;
    case MOSI_CACHE_IS: 
        do_cache_IS (request); 
        break;
    case MOSI_CACHE_S: 
        //fprintf (stderr, "\nState S\n");
        do_cache_S (request); 
        break;
    case MOSI_CACHE_SM: 
        do_cache_SM (request); 
        break;
    case MOSI_CACHE_O: 
        //fprintf (stderr, "\nState O\n");
        do_cache_O (request); 
        break;
    case MOSI_CACHE_OM: 
        do_cache_OM (request); 
        break;        
    case MOSI_CACHE_M: 
        //fprintf (stderr, "\nState M\n"); 
        do_cache_M (request); 
        break;
    default:
        fatal_error ("Invalid Cache State for MESI Protocol\n");
    }

}

void MOSI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MOSI_CACHE_I:  do_snoop_I (request); break;
    case MOSI_CACHE_IM: do_snoop_IM (request); break;
    case MOSI_CACHE_IS: do_snoop_IS (request); break;
    case MOSI_CACHE_S: do_snoop_S (request); break;
    case MOSI_CACHE_SM: do_snoop_SM (request); break;
    case MOSI_CACHE_O: do_snoop_O (request); break;
    case MOSI_CACHE_OM: do_snoop_OM (request); break;
    case MOSI_CACHE_M:  do_snoop_M (request); break;
    default:
    	fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MOSI_protocol::do_cache_I (Mreq *request)
{
        switch (request->msg) {
    // If we get a request from the processor we need to get the data
    case LOAD:
        /* Line up the GETS in the Bus' queue */
        send_GETS(request->addr);
        /* The IM state means that we have sent the GETS message and we are now waiting
    	 * on DATA
    	 */
    	 state = MOSI_CACHE_IS;
    	 /* This is a cache miss */
    	 Sim->cache_misses++;
    	 break;
    case STORE:
    	/* Line up the GETM in the Bus' queue */
    	send_GETM(request->addr);
    	/* The IM state means that we have sent the GETM message and we are now waiting
    	 * on DATA
    	 */
    	state = MOSI_CACHE_IM;
    	/* This is a cache miss */
    	Sim->cache_misses++;
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_IM (Mreq *request)
{
	switch (request->msg) {
	/* If the block is in the IM state that means it sent out a GETM message
	 * and is waiting on DATA.  Therefore the processor should be waiting
	 * on a pending request. Therefore we should not be getting any requests from
	 * the processor.
	 */
	case LOAD:
	
	case STORE:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error("Should only have one outstanding request per processor!");
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOSI_protocol::do_cache_IS (Mreq *request)
{
	switch (request->msg) {
	/* If the block is in the IM state that means it sent out a GETS message
	 * and is waiting on DATA.  Therefore the processor should be waiting
	 * on a pending request. Therefore we should not be getting any requests from
	 * the processor.
	 */
	case LOAD:
	
	case STORE:
		request->print_msg (my_table->moduleID, "ERROR");
		fatal_error("Should only have one outstanding request per processor!");
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
	}
}

inline void MOSI_protocol::do_cache_S (Mreq *request)
{
        switch (request->msg) {
    // If we get a request from the processor we need to get the data
    case LOAD:
        // I am in S and someone is reading to me, I send data to proc
        send_DATA_to_proc(request->addr);
        state = MOSI_CACHE_S;
        //fprintf (stderr, "\nState S\n");
        /* This is a cache hit */
        break;
    case STORE:
    	/* When I am in S state and someone is writing to me
    	 * Data will be written to me and I become M
    	 * and invalidate all other S <- done by snooping
    	 */
    	send_GETM(request->addr);
    	state = MOSI_CACHE_SM;
    	// cache miss
    	Sim->cache_misses++;
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_SM (Mreq *request)
{
        switch (request->msg) {
    
    case LOAD:
        
        
    case STORE:
    	// shouldn't come here
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_O (Mreq *request)
{
	switch (request->msg) {
	case LOAD:
	        // I am in O and someone reads from me, I send data to proc
	        send_DATA_to_proc(request->addr);
	        state = MOSI_CACHE_O;
	        //fprintf (stderr, "\nState O\n");
	        break;
	case STORE:
	        // I am in O and someone wants to write to me
	        // So I send a GETM for everyone to invalidate their copies and wait for them and go to M
	        send_GETM(request->addr);
		state = MOSI_CACHE_OM;
		//fprintf (stderr, "\nState M\n");
		Sim->cache_misses++;
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: O state shouldn't see this message\n");
	}
}

inline void MOSI_protocol::do_cache_OM (Mreq *request)
{
        switch (request->msg) {
    
    case LOAD:
        
        
    case STORE:
    	// shouldn't come here
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: O state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_cache_M (Mreq *request)
{
        // The M state means we have the data and we can modify it.  Therefore any request
        // from the processor (read or write) can be immediately satisfied.
	switch (request->msg) {
	case LOAD:
	        // When I am in M and I get a read, I remain in M
	        send_DATA_to_proc(request->addr);
	        state = MOSI_CACHE_M;
	        //fprintf (stderr, "\nState M\n");
	        break;
	case STORE:
	        // I am in M and someone writes to me, I remain in M
		send_DATA_to_proc(request->addr);
		state = MOSI_CACHE_M;
		//fprintf (stderr, "\nState M\n");
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
	}
}

inline void MOSI_protocol::do_snoop_I (Mreq *request)
{
    switch (request->msg) {
    case GETS:
    case GETM:
    case DATA:
    	/**
    	 * If we snoop a message from another cache and we are in I, then we don't
    	 * need to do anything!  We obviously cannot supply data since we don't have
    	 * it, and we don't need to downgrade our state since we are already in I.
    	 */
    	break;

    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_IM (Mreq *request)
{
        switch (request->msg) {
    case GETS:
    case GETM:
        /** While in IM we will see our own GETS or GETM on the bus.  We should just
	 * ignore it and wait for DATA to show up.
	 */
	break;
    case DATA:
	/** IM state meant that the block had sent the GETM and was waiting on DATA.
	 * Now that Data is received we can send the DATA to the processor and finish
	 * the transition to M.
	 */
	/**
	 * Note we use get_shared_line() here to demonstrate its use.
	 * (Hint) The shared line indicates when another cache has a copy and is useful
	 * for knowing when to go to the E/S state.
	 */
	send_DATA_to_proc(request->addr);
	state = MOSI_CACHE_M;
	//fprintf (stderr, "\nState M\n");
	if (get_shared_line())
	{
		// nothing to do now
	}
	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_IS (Mreq *request)
{
        switch (request->msg) {
    case GETS:
    case GETM: 

        /** While in IM we will see our own GETS or GETM on the bus.  We should just
	 * ignore it and wait for DATA to show up.
	 */
	break;    
    case DATA:
 	/** IS state meant that the block had sent the GETS and was waiting on DATA.
	 * Now that Data is received we can send the DATA to the processor and finish
	 * the transition to E/S.
	 */
	/**
	 * Note we use get_shared_line() here to demonstrate its use.
	 * (Hint) The shared line indicates when another cache has a copy and is useful
	 * for knowing when to go to the E/S state.
	 */
	send_DATA_to_proc(request->addr);
	state = MOSI_CACHE_S;
	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_S (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am in S and I see a GetS, I set shared line
        set_shared_line();
        //fprintf(stderr,"**** In do_snoop_S ******");
        break;
    case GETM:
        // I am in S and I see a getM, I invalidate my entry
        state = MOSI_CACHE_I;
        //fprintf (stderr, "\nState I\n");
        break;
    case DATA:
    	// I don't have to wait for data when I'm in S because I already have it!
    	
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_SM (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am not yet completely M so when I see a GetS, I'll set shared line
        set_shared_line();
        break;
    case GETM:
     	break;
    case DATA:
     	// Once I get data, I'll send to proc and become M
    	send_DATA_to_proc(request->addr);
    	state = MOSI_CACHE_M;
    	//fprintf (stderr, "\nState M\n");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_O (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am in O and I see a GETS
        // Then I will put the value on the bus = cache to cache transfer
        send_DATA_on_bus(request->addr,request->src_mid);
        set_shared_line();
        //fprintf (stderr, "\nState S\n");
        break;
    case GETM:
        // I am in O and I see a GETM
        // Someone else is trying to modify that data
        // So I have to go to Inv
        send_DATA_on_bus(request->addr,request->src_mid);
        state = MOSI_CACHE_I;
        //fprintf (stderr, "\nThis State I\n");
        break;
    case DATA:
    	// don't have to do anything because I already have it!
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: O state shouldn't see this message\n");
    }
}

inline void MOSI_protocol::do_snoop_OM (Mreq *request)
{
       switch (request->msg) {
    case GETS:
        // I am not yet M, so when I see a GetS, I set shared line and I am still an owner
        send_DATA_on_bus(request->addr, request->src_mid);
        set_shared_line();
        break;
    case GETM:
        // I am in O and I see a getM
        // I am not yet M, so I am still an owner
        send_DATA_on_bus(request->addr, request->src_mid);
        set_shared_line();
        state = MOSI_CACHE_IM;
     	break;
    case DATA:
     	// when I see data, I send it to proc and I become M
    	//send_DATA_to_proc(request->addr);
    	//state = MOSI_CACHE_M;
    	
    	//fprintf (stderr, "\nState M\n");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}


inline void MOSI_protocol::do_snoop_M (Mreq *request)
{
       switch (request->msg) {
    case GETS:
        // I am in M and I see a GetS
        // I become owner! and I give data

        send_DATA_on_bus(request->addr,request->src_mid);
        set_shared_line(); 
        state = MOSI_CACHE_O;
        //fprintf (stderr, "\nState S\n");
        
        break;
    case GETM:
        // I am in M and I see a GetM, then I update memory and I go to invalidate
        
        send_DATA_on_bus(request->addr,request->src_mid);       
        state = MOSI_CACHE_I;
        //fprintf (stderr, "\nState I\n");
        break;
    case DATA:
    	// no use of data waiting
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
    }
}

