#include "cachesim.hpp"
#include "time.h"
#include <cstdio>
#include <math.h>


 // Subroutine for initializing the cache. You many add and initialize any global or heap
 // variables as needed.
 // XXX: You're responsible for completing this routine


class Cache
{
public:
    bool dirty_bit;
    bool valid_bit;
    bool prefetched;
    uint64_t tag;
    uint64_t timestamp;
    Cache()
    {
        dirty_bit = false;
        valid_bit= false;
        prefetched = false;
        tag = 0;
        timestamp = 100;
    }
   
};

uint64_t block_bits;
uint64_t cache_bits;
uint64_t associativity;
uint64_t set_size;
uint64_t victim_cache_size;
uint64_t degree_prefetch;
uint64_t last_miss_block_address;
uint64_t pending_stride;
uint64_t counter=100;
Cache* cache;
Cache* victim_cache;

// @c The total number of bytes for data storage is 2^C
// @b The size of a single cache line in bytes is 2^B
// @s The number of blocks in each set is 2^S
// @v The number of blocks in the victim cache is 2^V
// @k The prefetch distance is K
 
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, uint64_t k)
{
   uint64_t cache_size = pow(2,(c-b));
   cache = new Cache[cache_size]; //2^(c-b) blocks
   victim_cache = new Cache[v]; //v blocks

    block_bits = b;
    cache_bits = c;
    associativity = s;
    set_size = pow(2,(c-b-s)); // no. of sets
    victim_cache_size = v;
    degree_prefetch = k;
    last_miss_block_address =0;
    pending_stride=0;
return;
}

void LRU(Cache* cache, uint64_t index,uint64_t& LRUindex)
{
    uint64_t temp;
    temp = cache[index].timestamp;
    LRUindex = index;

    for(uint64_t i=0; i<pow(2,associativity);i++)
    {
      if(temp > cache[index+i*(set_size)].timestamp && cache[index+i*(set_size)].valid_bit== true)
      {
      temp=cache[index+i*(set_size)].timestamp;
      LRUindex = index+i*(set_size);
      }
    }
return;
}

void FIFO(Cache* victim_cache,uint64_t& First_in)
{
    uint64_t temp;
    for(uint64_t i =0; i<victim_cache_size;i++)
    {
     if(victim_cache[i].valid_bit == false) 
     {First_in = i;return;}
    }      
    for(uint64_t i=0; i<victim_cache_size;i++)
    {
      if(victim_cache[i].valid_bit ==true)
      {
      temp = victim_cache[i].timestamp;
      First_in=i;
      break;
      }
    }
    
    for(uint64_t i =0; i<victim_cache_size;i++)
    {
     if(temp > victim_cache[i].timestamp && victim_cache[i].valid_bit == true)
     {
     temp = victim_cache[i].timestamp;
     First_in = i;
     }
    }
  return;
}

void Block_Address(uint64_t address, uint64_t& block_addr)
{
    block_addr= address>>(block_bits);
    block_addr=block_addr<<(block_bits);
return;
}


uint64_t subtract(uint64_t a, uint64_t b, int& neg)
{
    if(a>b) {neg = 1;return (a-b);}
    else {neg = -1;return (b-a);}
}

void strided_prefetch(uint64_t address, Cache* cache, Cache* victim_cache, cache_stats_t* p_stats)
{
    uint64_t block_address =0;
    uint64_t d =0;
    int neg=1;
    Block_Address(address, block_address);
    d = subtract(block_address, last_miss_block_address, neg);
    last_miss_block_address = block_address;
   
    if(d*neg == pending_stride) // fetch the next k blocks
    {
        for(uint64_t j=1;j<=(degree_prefetch);j++)
        {
            if((block_address+d*neg)<0) {printf("yeslssr");
                return;}
            if((block_address+d*neg)>(pow(2,64) -1)){printf("yesgrtr");
                return;}
            
            uint64_t index_mask_bits =(cache_bits)-(block_bits)-(associativity); //# of index bits = c-b-s
            uint64_t index_mask = (pow(2,index_mask_bits)-1);
            
            uint64_t fetch_address = block_address+d*j*neg;
            uint64_t fetch_tag_and_index = fetch_address>>(block_bits);
            uint64_t fetch_index = fetch_tag_and_index & index_mask;
            uint64_t fetch_tag = fetch_address>>((cache_bits)-(associativity));
            bool proceed=false, proceed1 = false, proceed2 = false; 


            for(uint64_t i=0; i<pow(2,associativity);i++) //check L1 while prefetching
            {
                if(cache[fetch_index+i*(set_size)].tag == fetch_tag && cache[fetch_index+i*(set_size)].valid_bit == true)
                {p_stats->prefetched_blocks++;proceed = true;break;}
            }
            if(proceed == true)continue;
            

            if(victim_cache_size>0) //check victim cache while prefetching
	    {
            for(uint64_t i=0; i< victim_cache_size; i++)
            {
              if(victim_cache[i].tag == fetch_tag_and_index && victim_cache[i].valid_bit == true) // if found in victim cache
              { // swap vc entry with least recent L1 cache entry
		Cache temp;
                uint64_t LRUindex;
                if(associativity>0) LRU(cache, fetch_index, LRUindex);                  
                if(associativity == 0) LRUindex=fetch_index;  
                temp = cache[LRUindex];
	
                uint64_t LRU_timestamp = cache[LRUindex].timestamp;
                temp.tag = (temp.tag)<<index_mask_bits;
                temp.tag = (temp.tag)|(fetch_index);
                temp.timestamp = victim_cache[i].timestamp;            
	        temp.valid_bit = true;

                victim_cache[i].tag = (victim_cache[i].tag) >> index_mask_bits;
            
                cache[LRUindex] = victim_cache[i];
                victim_cache[i] = temp;
            
                if(LRU_timestamp==0) {//printf("SEVEN HELLS!!");		  
        	cache[LRUindex].timestamp = 0;}else 	  
                cache[LRUindex].timestamp = LRU_timestamp-1;
       	        cache[LRUindex].prefetched = true;
                p_stats->prefetched_blocks++;
                victim_cache[i].valid_bit = true;
                proceed1 = true;
            
                break;
                }
             }
             }
            if(proceed1 == true) continue;

            for(uint64_t i=0; i<pow(2,associativity);i++)
            {
                if(cache[fetch_index+i*(set_size)].valid_bit == false)
		{
		   uint64_t LRUindex;
            	   if(associativity>0)LRU(cache, fetch_index, LRUindex);
            	   if(associativity==0) LRUindex = fetch_index;
	           uint64_t temp_timestamp1 = cache[LRUindex].timestamp;

                   cache[fetch_index+i*(set_size)].tag = fetch_tag;
 	 	   cache[fetch_index+i*(set_size)].valid_bit = true;
 	   	   cache[fetch_index+i*(set_size)].prefetched = true;
		   if(temp_timestamp1==0) {		  
	           cache[fetch_index+i*(set_size)].timestamp = 0;}else 	  
	           cache[fetch_index+i*(set_size)].timestamp = temp_timestamp1-1;
 		   cache[fetch_index+i*(set_size)].dirty_bit = false;
		   p_stats->prefetched_blocks++;
                   proceed2=true;
                   break;
		 }
             }
            if(proceed2 == true) continue;
	    
	    uint64_t LRUindex;
            if(victim_cache_size>0) //put LRU in victim cache, kickout first in entry from VC
            {
 	      uint64_t First_in;
              FIFO(victim_cache,First_in);
              if(associativity>0)LRU(cache, fetch_index, LRUindex);
              if(associativity==0) LRUindex = fetch_index;
	      counter++;
              
	      if(victim_cache[First_in].dirty_bit == true && victim_cache[First_in].valid_bit == true) p_stats->write_backs++;
              victim_cache[First_in]=cache[LRUindex];
              victim_cache[First_in].tag = ((cache[LRUindex].tag)<<index_mask_bits) | fetch_index;
              victim_cache[First_in].timestamp = counter;
              victim_cache[First_in].valid_bit = true;
            }
            //insert incoming address in place of LRU
                    
	    if(associativity>0)LRU(cache, fetch_index, LRUindex);
            if(associativity==0) LRUindex = fetch_index;
            uint64_t temp_timestamp2 = cache[LRUindex].timestamp;

            if(victim_cache_size == 0){
            if(cache[LRUindex].dirty_bit == true && cache[LRUindex].valid_bit == true) p_stats->write_backs++;}
            
            cache[LRUindex].tag = fetch_tag;
            if(temp_timestamp2==0) {	  
	    cache[LRUindex].timestamp = 0;}else 	  
            cache[LRUindex].timestamp = temp_timestamp2-1; //LRU
            cache[LRUindex].valid_bit=true;
            cache[LRUindex].prefetched=true;
            cache[LRUindex].dirty_bit = false;
            p_stats->prefetched_blocks++;   
        }
    }
    pending_stride = d*neg;
return;
}


void check_cache(Cache* cache, Cache* victim_cache, uint64_t address, char rw, cache_stats_t* p_stats)
{
    uint64_t index_mask_bits =(cache_bits)-(block_bits)-(associativity); //# of index bits = c-b-s
    uint64_t index_mask = pow(2,index_mask_bits)-1;
    uint64_t tag_and_index = address>>(block_bits);
    uint64_t index = tag_and_index & index_mask;
    uint64_t tag = address>>((cache_bits)-(associativity)); //tag = remove (c-s) bits from address
    
    //check L1 cache
    for(uint64_t i=0; i<pow(2,associativity);i++)
        {
        if(cache[index+i*(set_size)].tag == tag && cache[index+i*(set_size)].valid_bit == true)
          {
           cache[index+i*(set_size)].timestamp = counter;
           if(cache[index+i*(set_size)].prefetched==true) {p_stats->useful_prefetches++; cache[index+i*(set_size)].prefetched=false;}
           if(rw=='w') cache[index+i*(set_size)].dirty_bit=true;                    
           return;
          }
        }

    p_stats->misses++;
    if(rw == 'r')
    p_stats->read_misses++;
    if(rw == 'w')
    p_stats->write_misses++;

    if(victim_cache_size >0) //check victim cache
      {
    for(uint64_t i=0; i<victim_cache_size;i++)
        {
         if(victim_cache[i].tag == tag_and_index && victim_cache[i].valid_bit == true)  // Hit in VC
          {// swap vc entry with least recent L1 cache entry
           Cache temp;
           uint64_t LRUindex;
           if(associativity>0) LRU(cache, index, LRUindex);
           if(associativity == 0) LRUindex=index;  
           temp = cache[LRUindex];
           temp.tag = (temp.tag)<<index_mask_bits;
           temp.tag = (temp.tag)|(index);
           temp.timestamp = victim_cache[i].timestamp;            
	   temp.valid_bit = true;

           victim_cache[i].tag = (victim_cache[i].tag) >> index_mask_bits;

           cache[LRUindex] = victim_cache[i];
           victim_cache[i] = temp;
            
           if(cache[LRUindex].prefetched==true){ p_stats->useful_prefetches++; cache[LRUindex].prefetched = false;}
           if(rw=='w') cache[LRUindex].dirty_bit = true;
            
           cache[LRUindex].timestamp = counter;
           victim_cache[i].valid_bit = true;
	    
           if(degree_prefetch > 0 )
           strided_prefetch(address,cache,victim_cache,p_stats);
           return;
          }
        }
      }
    // if not found in L1 cache or victim cache, update miss data
    if(rw =='r')
    p_stats->read_misses_combined++;
    if(rw == 'w')
    p_stats->write_misses_combined++;
    p_stats->vc_misses++;

    for(uint64_t i=0; i<pow(2,associativity);i++) // check if there is still room in L1
       {
	if(cache[index+i*(set_size)].valid_bit == false)
          {
           cache[index+i*(set_size)].tag=tag;
           cache[index+i*(set_size)].valid_bit=true;
           cache[index+i*(set_size)].timestamp= counter;
           if(rw=='w') cache[index+i*(set_size)].dirty_bit = true;
	   if(rw=='r') cache[index+i*(set_size)].dirty_bit = false;
	   cache[index+i*(set_size)].prefetched=false;
           if(degree_prefetch > 0)
           strided_prefetch(address,cache,victim_cache,p_stats);
           return;
          }
        } 
    
    uint64_t LRUindex;
    if(associativity>0)LRU(cache, index, LRUindex);
    if(associativity == 0) LRUindex = index;
           
    //put LRU in victim cache, kickout first in entry from VC
           
    if(victim_cache_size >0)
       {
	 uint64_t First_in;
         FIFO(victim_cache,First_in);
          
         if(victim_cache[First_in].dirty_bit==true && victim_cache[First_in].valid_bit == true) p_stats->write_backs++;
           
         victim_cache[First_in]=cache[LRUindex];
         victim_cache[First_in].valid_bit = true;
         victim_cache[First_in].tag = ((cache[LRUindex].tag)<<index_mask_bits) | (index);
         victim_cache[First_in].timestamp = counter;           
       }
    //insert incoming address in place of LRU
    if(victim_cache_size == 0)   {
    if(cache[LRUindex].dirty_bit==true && cache[LRUindex].valid_bit == true) p_stats->write_backs++;}
           
    cache[LRUindex].tag = tag;
    cache[LRUindex].valid_bit=true;
    cache[LRUindex].prefetched = false;
    if(rw == 'w') cache[LRUindex].dirty_bit = true;
    if(rw == 'r') cache[LRUindex].dirty_bit = false;	
    cache[LRUindex].timestamp = counter;
    if(degree_prefetch > 0 )
    strided_prefetch(address,cache,victim_cache,p_stats);
    return;
}

// Subroutine that simulates the cache one trace event at a time.
// XXX: You're responsible for completing this routine
// @rw The type of event. Either READ or WRITE
// @address  The target memory address
// @p_stats Pointer to the statistics structure

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats)
{
   p_stats->accesses++;
   counter++;
    if(rw == 'r')
    {
        p_stats->reads++;
        check_cache(cache, victim_cache, address,rw, p_stats);
    }
    if(rw == 'w')
    {
        p_stats->writes++;
        check_cache(cache, victim_cache, address,rw, p_stats);
    }
return;
}

// Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
// such as miss rate or average access time.
// XXX: You're responsible for completing this routine
// @p_stats Pointer to the statistics structure
 
void complete_cache(cache_stats_t *p_stats)
{
(p_stats->hit_time)=2+0.2*associativity;
(p_stats->miss_penalty)=200;
(p_stats->miss_rate)=(double)(p_stats->misses)/(p_stats->accesses);
double vc_miss_rate = (double)(p_stats->vc_misses)/(p_stats->accesses);
(p_stats->avg_access_time)=(p_stats->hit_time)+vc_miss_rate*(p_stats->miss_penalty);
(p_stats->bytes_transferred) = ((p_stats->vc_misses)+(p_stats ->write_backs)+(p_stats->prefetched_blocks))*pow(2,block_bits);
return;
}
