#include "MESI_protocol.h"
#include "../sim/mreq.h"
#include "../sim/sim.h"
#include "../sim/hash_table.h"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_protocol::MESI_protocol (Hash_table *my_table, Hash_entry *my_entry)
    : Protocol (my_table, my_entry)
{
        this->state = MESI_CACHE_I;
}

MESI_protocol::~MESI_protocol ()
{    // Someone else is trying to modify that data
       
}

void MESI_protocol::dump (void)
{
    const char *block_states[8] = {"X","I","IM","IS","S","SM","E","M"};
    fprintf (stderr, "MESI_protocol - state: %s\n", block_states[state]);
}

void MESI_protocol::process_cache_request (Mreq *request)
{
   switch (state) {
    case MESI_CACHE_I:  
        //fprintf (stderr, "\nState I\n");
        do_cache_I (request); 
        break;
    case MESI_CACHE_IM: 
        do_cache_IM (request); 
        break;
    case MESI_CACHE_IS: 
        do_cache_IS (request); 
        break;
    case MESI_CACHE_S: 
        //fprintf (stderr, "\nState S\n");
        do_cache_S (request); 
        break;
    case MESI_CACHE_SM: 
        do_cache_SM (request); 
        break;
    case MESI_CACHE_E: 
        //fprintf (stderr, "\nState E\n");
        do_cache_E (request); 
        break;
    case MESI_CACHE_M: 
        //fprintf (stderr, "\nState M\n"); 
        do_cache_M (request); 
        break;
    default:
        fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

void MESI_protocol::process_snoop_request (Mreq *request)
{
	switch (state) {
    case MESI_CACHE_I:  do_snoop_I (request); break;
    case MESI_CACHE_IM: do_snoop_IM (request); break;
    case MESI_CACHE_IS: do_snoop_IS (request); break;
    case MESI_CACHE_S: do_snoop_S (request); break;
    case MESI_CACHE_SM: do_snoop_SM (request); break;
    case MESI_CACHE_E: do_snoop_E (request); break;
    case MESI_CACHE_M:  do_snoop_M (request); break;
    default:
    	fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MESI_protocol::do_cache_I (Mreq *request)
{
        switch (request->msg) {
    // If we get a request from the processor we need to get the data
    case LOAD:
        /* Line up the GETS in the Bus' queue */
        send_GETS(request->addr);
        /* The IM state means that we have sent the GETS message and we are now waiting
    	 * on DATA
    	 */
    	 state = MESI_CACHE_IS;
    	 /* This is a cache miss */
    	 Sim->cache_misses++;
    	 break;
    case STORE:
    	/* Line up the GETM in the Bus' queue */
    	send_GETM(request->addr);
    	/* The IM state means that we have sent the GETM message and we are now waiting
    	 * on DATA
    	 */
    	state = MESI_CACHE_IM;
    	/* This is a cache miss */
    	Sim->cache_misses++;
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_cache_IM (Mreq *request)
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

inline void MESI_protocol::do_cache_IS (Mreq *request)
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

inline void MESI_protocol::do_cache_S (Mreq *request)
{
        switch (request->msg) {
    // If we get a request from the processor we need to get the data
    case LOAD:
        // I am in S and someone reads from me, so I send data to proc
        send_DATA_to_proc(request->addr);
        state = MESI_CACHE_S;
        //fprintf (stderr, "\nState S\n");
        /* This is a cache hit */
        break;
    case STORE:
    	// I am in S and someone is writing to me, so I send out GetM and go to SM
    	// I am going to wait for everyone to be done invalidating
    	
    	send_GETM(request->addr);
    	state = MESI_CACHE_SM;
    	// cache miss
    	Sim->cache_misses++;
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_cache_SM (Mreq *request)
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

inline void MESI_protocol::do_cache_E (Mreq *request)
{
	switch (request->msg) {
	// when I am in E and someone is reading from me, I remain in E and send data to proc
	case LOAD:
	        send_DATA_to_proc(request->addr);
	        state = MESI_CACHE_E;
	        //fprintf (stderr, "\nState E\n");
	        break;
        // when I see a write, I go to M and no need to broadcast as no other cache has a copy
	case STORE:
	        send_DATA_to_proc(request->addr);
		state = MESI_CACHE_M;
		//fprintf (stderr, "\nState M\n");
		// silent upgrade
		Sim->silent_upgrades++;
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: E state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_cache_M (Mreq *request)
{
        // The M state means we have the data and we can modify it.  Therefore any request
        // from the processor (read or write) can be immediately satisfied.
	switch (request->msg) {
	// When I am in M and I get a read, I remain in M and send data to proc
	case LOAD:
	        send_DATA_to_proc(request->addr);
	        state = MESI_CACHE_M;
	        //fprintf (stderr, "\nState M\n");
	        break;
        // Note: There was no need to send anything on the bus on a hit.
	case STORE:
	        // I am in M and someone is writing to me, I send out data to proc and remain in M
		send_DATA_to_proc(request->addr);
		state = MESI_CACHE_M;
		//fprintf (stderr, "\nState M\n");
		break;
	default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: M state shouldn't see this message\n");
	}
}

inline void MESI_protocol::do_snoop_I (Mreq *request)
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

inline void MESI_protocol::do_snoop_IM (Mreq *request)
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
	state = MESI_CACHE_M;
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

inline void MESI_protocol::do_snoop_IS (Mreq *request)
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
	
	if (get_shared_line())
	{
		state = MESI_CACHE_S;
		//fprintf (stderr, "\nState S\n");	
	}
	else
	{
	        state = MESI_CACHE_E;  

	        //fprintf (stderr, "\nState E\n");     
	}
	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: I state shouldn't see this message\n");
    }
}


inline void MESI_protocol::do_snoop_S (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am in S and I see a GetS, I set shared line
        set_shared_line();
        break;
    case GETM:
        // I am in S and I see a getM so I invalidate myself
        state = MESI_CACHE_I;
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

inline void MESI_protocol::do_snoop_SM (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // Now I haven't yet become M, so I still need to set shared line when I see a GetS
        set_shared_line();
        break;
    case GETM:
     	break;
    case DATA:
     	// I send data to proc now and go to M as everyone is done invalidating
    	send_DATA_to_proc(request->addr);
    	state = MESI_CACHE_M;
    	//fprintf (stderr, "\nState M\n");
        break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_snoop_E (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am in E and I see a GETS
        // Then I have to become S and set_shared_line because after GETS, someone else will also have this block's copy
        send_DATA_on_bus(request->addr,request->src_mid);
        set_shared_line();
        state = MESI_CACHE_S;
        //fprintf (stderr, "\nState S\n");
        break;
    case GETM:
        // I am in E and I see a GETM
        // Someone else is trying to modify that data
        // So I have to go to Inv
        send_DATA_on_bus(request->addr,request->src_mid);
        state = MESI_CACHE_I;
        //fprintf (stderr, "\nThis State I\n");
        break;
    case DATA:
    	// don't have to do anything because I already have it!
    	break;
    default:
        request->print_msg (my_table->moduleID, "ERROR");
        fatal_error ("Client: E state shouldn't see this message\n");
    }
}

inline void MESI_protocol::do_snoop_M (Mreq *request)
{
        switch (request->msg) {
    case GETS:
        // I am in M and I see a GETS. Then I have to halt and first update the memory and become S
        // I also will have to set_shared_line because multiple caches will have this copy

        send_DATA_on_bus(request->addr,request->src_mid);
        set_shared_line(); 
        state = MESI_CACHE_S;
        //fprintf (stderr, "\nState S\n");
        
        break;
    case GETM:
        // I am in M and I see a GETM. Then I will have to update the memory because I dont want conflict of bytes in block
        // After I update the memory, this data will be modified. So I will be Invalid
        send_DATA_on_bus(request->addr,request->src_mid);       
        state = MESI_CACHE_I;
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

