/* page-collect.c -- collect a snapshot each of of the /proc/pid/maps files,
 *      with each VM region interleaved with a list of physical addresses
 *      which make up the virtual region.
 * Copyright C2009 by EQware Engineering, Inc.
 *
 *    page-collect.c is part of PageMapTools.
 *
 *    PageMapTools is free software: you can redistribute it and/or modify
 *    it under the terms of version 3 of the GNU General Public License
 *    as published by the Free Software Foundation
 *
 *    PageMapTools is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with PageMapTools.  If not, see http://www.gnu.org/licenses.
 */

#define _LARGEFILE64_SOURCE

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <stdint.h>
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <math.h>

#include "page-collect.h"

// ERR() --
#define ERR(format, ...) fprintf(stderr, format, ## __VA_ARGS__)


// Regular_TLB - fixed size entry 4K or 2M
uint64_t Regular_TLB_4K = 0;
uint64_t Regular_TLB_2M = 0;


static struct option opts[] = {
    { "pid"       , 1, NULL, 'p' },
    { "out-file"  , 1, NULL, 'f' },
    { "help"      , 0, NULL, 'h' },
    { NULL        , 0, NULL, 0 }
};

bool pairCompare(const std::pair<int64_t, uint64_t>& firstElem, const std::pair<int64_t, uint64_t>& secondElem) {
    return firstElem.second > secondElem.second;

}

bool pairCompareMin(const std::pair<int64_t, uint64_t>& firstElem, const std::pair<int64_t, uint64_t>& secondElem) {
    return firstElem.second < secondElem.second;

}

// is_switch() --
static inline bool is_switch(char c)
{
    return c == '-';
}

// is_directory() --
static bool is_directory(const char *dirname)
{
    struct stat buf;
    int n;

    assert(dirname != NULL);

    n = stat(dirname, &buf);

    return (n == 0 && (buf.st_mode & S_IFDIR) != 0)? TRUE: FALSE;

}

// is_wholly_numeric() --
static bool is_wholly_numeric(const char *str)
{
    assert(str != NULL);

    while (*str != '\0')
    {
        if (!isdigit(*str))
        {
            return FALSE;
        }
        str++;
    }
    return TRUE;

}


// print the content of the histogram and
// generate and print the cumulative distribution function in %
void print_range_hist(FILE *out, std::map<uint64_t, uint64_t, std::greater<uint64_t> > &hist, uint64_t total_present_pages, const char* outstring)
{

	uint64_t num_entries(0), num_entries_80_percent_coverage(0), num_entries_90_percent_coverage(0), num_entries_99_percent_coverage(0);
	uint64_t current_coverage(0), _32_entries_coverage(0), _64_entries_coverage(0), _128_entries_coverage(0), _256_entries_coverage(0), _80_percent_coverage(0), _90_percent_coverage(0), _99_percent_coverage(0);

	uint64_t remaining_pages;
	uint64_t required_entries;

	fprintf(out, "\n----------\n");
	fprintf(out, "%s\n", outstring);
	fprintf(out, "----------\n");

	if (hist.size()) {

		//total_ranges = std::accumulate(std::begin(hist), std::end(hist), 0, [](const std::size_t previous, const std::pair<const std::string, std::size_t>& p){ return previous + p.second; });					                                                
		for (std::map<uint64_t, uint64_t>::iterator it=hist.begin(); it!=hist.end(); ++it) {

			if (((num_entries + it->second) > 32) && !_32_entries_coverage)
  				_32_entries_coverage = current_coverage + (32 - num_entries)*it->first;       
			if (((num_entries + it->second) > 64) && !_64_entries_coverage)
  				_64_entries_coverage = current_coverage + (64 - num_entries)*it->first;       
			if (((num_entries + it->second) > 128) && !_128_entries_coverage)
  				_128_entries_coverage = current_coverage + (128 - num_entries)*it->first;       
			if (((num_entries + it->second) > 256) && !_256_entries_coverage)
  				_256_entries_coverage = current_coverage + (256 - num_entries)*it->first;       

			if(((float)(current_coverage + it->first*it->second)/total_present_pages) >=0.8 && !num_entries_80_percent_coverage){
				remaining_pages = ceil(0.8*(float)total_present_pages)-current_coverage;
				required_entries = ceil((float)remaining_pages/it->first);
				num_entries_80_percent_coverage = 	num_entries + required_entries;
				_80_percent_coverage = current_coverage + required_entries * it->first;
			}
            
			if(((float)(current_coverage + it->first*it->second)/total_present_pages) >=0.9 && !num_entries_90_percent_coverage){
				remaining_pages = ceil(0.9*(float)total_present_pages)-current_coverage;
				required_entries = ceil((float)remaining_pages/it->first);
				num_entries_90_percent_coverage = num_entries + required_entries;
				_90_percent_coverage = current_coverage + required_entries * it->first;
			}

			if(((float)(current_coverage + it->first*it->second)/total_present_pages) >=0.99 && !num_entries_99_percent_coverage){
				remaining_pages = ceil(0.99*(float)total_present_pages)-current_coverage;
				required_entries = ceil((float)remaining_pages/it->first);
				num_entries_99_percent_coverage = num_entries + required_entries;
				_99_percent_coverage = current_coverage + required_entries * it->first;
			}

			num_entries+=it->second;
			current_coverage+= it->first * it->second; 
			//fprintf(out, "range_%s_hist: %ld %ld %0.2f %ld\n", outstring, it->first, it->second, 100*((float) cur_contiguity / total_present_pages));
		}

		fprintf(out, "total_%s_entries: %ld\n", outstring, num_entries);
		fprintf(out, "total_%s_coverage: %ld (MB)\n", outstring, current_coverage * PAGE_SIZE / 1024/ 1024);
		fprintf(out, "\n");


		fprintf(out, "32 entries coverage: %0.2f%% (%lu 4K pages)\n", 100*((float)_32_entries_coverage/total_present_pages), _32_entries_coverage);
		fprintf(out, "64 entries coverage: %0.2f%% (%lu 4K pages)\n", 100*((float)_64_entries_coverage/total_present_pages), _64_entries_coverage);
		fprintf(out, "128 entries coverage: %0.2f%% (%lu 4K pages)\n", 100*((float)_128_entries_coverage/total_present_pages), _128_entries_coverage);
		fprintf(out, "256 entries coverage: %0.2f%% (%lu 4K pages)\n", 100*((float)_256_entries_coverage/total_present_pages), _256_entries_coverage);
		fprintf(out, "\n");

		fprintf(out, "Number of entries for at least 80%% coverage: %lu (exact coverage %0.2f%%)\n", num_entries_80_percent_coverage, 100*((float)_80_percent_coverage/total_present_pages));
		fprintf(out, "Number of entries for at least 90%% coverage: %lu (exact coverage %0.2f%%)\n", num_entries_90_percent_coverage, 100*((float)_90_percent_coverage/total_present_pages));
		fprintf(out, "Number of entries for at least 99%% coverage: %lu (exact coverage %0.2f%%)\n", num_entries_99_percent_coverage, 100*((float)_99_percent_coverage/total_present_pages));

	}
	else {
		fprintf(out, "\n\n");
		fprintf(out, "total_%s_entries: 0\n", outstring);
		fprintf(out, "total_%s_working_set: 0 (KB)\n", outstring);
		fprintf(out, "total_%s_working_percent: 0 (%%)\n", outstring);
		fprintf(out, "avg_%s_contiguity: 0\n", outstring);
		fprintf(out, "\n", outstring);
		fprintf(out, "range_%s_hist: 1 0 100\n", outstring);
	}
}




// usage() --
static void usage(void)
{
    fprintf(stderr,
        "usage: page-collect {switches}\n"
        "switches:\n"
        " -p pid          -- Collect only for process with $pid\n"
        " -o out-file     -- Output file name (def=%s)\n"
        "\n",
        OUT_NAME);
}



void update_anchor_tlb(std::map<uint64_t, uint64_t, std::greater<uint64_t> > &Anchor_TLB_hist, std::map<uint64_t, uint64_t, std::greater<uint64_t> > &Range_TLB_hist, unsigned long start_vpn, unsigned long end_vpn){

int anchor_distance = DEFAULT_ANCHOR_DISTANCE;
unsigned long aligned_vpn; 

#if DYNAMIC_ANCHOR_DISTANCE==TRUE
	// calculate dynamically the optimal anchor distance 
	// based on the algorithm described in the Hybrid TLB Coalescing paper
	// and using the Range TLB histogram (instead of the page tables) to calculate it
	// (still this is not an aqurate implementation, it is an estimation, however it is optimistic compared to the described design)
	int cost[MAX_ANCHOR_DISTANCE];
    int i;
    for (i=0; i<MAX_ANCHOR_DISTANCE; i++)
        cost[i]=0;

    for (i=0; i<MAX_ANCHOR_DISTANCE; i++){
        for (std::map<uint64_t, uint64_t>::iterator it=Range_TLB_hist.begin(); it!=Range_TLB_hist.end(); ++it) {
            cost[i]+=(it->first/(1<<i)+(it->first%(1<<i))/512 + (it->first%(1<<i))%512)*it->second;
        }
    }

    int min_cost = cost[0];

    for(i=1; i<MAX_ANCHOR_DISTANCE; i++){
        if(cost[i]<=min_cost){
            anchor_distance=i;
            min_cost = cost[i];
        }
    }

#endif

	aligned_vpn=start_vpn+((1<<anchor_distance)-(start_vpn%(1<<anchor_distance)));

	while (start_vpn < end_vpn) {
		if (start_vpn < aligned_vpn){
			if((start_vpn%512) || ((start_vpn+512)>end_vpn) || ((start_vpn+512)>aligned_vpn)){
				Anchor_TLB_hist[1]++;
				start_vpn+=1;
			}
			else {
				Anchor_TLB_hist[512]++;
				start_vpn+=512;
			}
		}
		else {
			assert(!(start_vpn%(1<<anchor_distance)));
			unsigned long temp_size_of_anchor_entry = ((1<<anchor_distance) < (end_vpn-start_vpn)) ? (1<<anchor_distance) : (end_vpn-start_vpn);
			Anchor_TLB_hist[temp_size_of_anchor_entry]++;
			start_vpn+=temp_size_of_anchor_entry;

		}
	}
}

// main() --
int main(int argc, char *argv[])
{
    struct dirent *de_qemu, *de_guest;
    int n;

    FILE *m   = NULL;
    int pm    = -1;
    FILE *out = NULL;
    DIR *proc = NULL;
    DIR *proc_guest = NULL;
    int retval = 0;
    int c;
    char *out_name = OUT_NAME;
    pid_t opt_guest_pid = 0;	/* process to walk */
    pid_t opt_qemu_pid = 0;	/* process to walk */

    uint64_t total_present_pages = 0;

    // VMA_TLB - contiguity only in VMAs
    std::map<uint64_t, uint64_t, std::greater<uint64_t> > VMA_TLB_hist;

    // Virtual_TLB - contiguity only in virtual memory
    std::map<uint64_t, uint64_t, std::greater<uint64_t> > Virtual_TLB_hist;

	// Range_TLB - contiguity in both virtual & physical memory (no alignment, RMM)
    std::map<uint64_t, uint64_t, std::greater<uint64_t> > Guest_Range_TLB_hist;
    
	// Range_TLB - contiguity in both virtual & physical memory (no alignment, RMM)
    std::map<uint64_t, uint64_t, std::greater<uint64_t> > Range_TLB_hist;
    
    std::map<uint64_t, uint64_t, std::greater<uint64_t> > Anchor_TLB_hist;
    
    /*ToDO: Describe*/
    std::map<uint64_t, uint64_t, std::greater <uint64_t> > SpOT_hist;
    std::map<uint64_t, uint64_t> Offsets;


    // Process command-line arguments.
    while ((c = getopt_long(argc, argv, "o:f:p:q:h", opts, NULL)) != -1) {
        switch (c) {
        case 'o':
            out_name = optarg;
            break;
        case 'p':
            opt_guest_pid = strtoll(optarg, NULL, 0);
            break;
        case 'q':
            opt_qemu_pid = strtoll(optarg, NULL, 0);
            break;
        case 'h':
            usage();
            exit(0);
        default:
            usage();
            exit(1);
        }
    }

    // Open output file for writing.
    out = fopen(out_name, "w");
    if (out == NULL) {
        ERR("Unable to open file \"%s\" for writing (errno=%d). (1)\n", out_name, errno);
        retval = -1;
        goto done;
    }


    // Open /proc directory for traversal.
    proc = opendir(PROC_DIR_NAME);
    if (proc == NULL) {
        ERR("Unable to open directory \"%s\" for traversal (errno=%d). (4)\n", PROC_DIR_NAME, errno);
        retval = -1;
        goto done;
    }

    // For each entry in the /proc directory...
    de_qemu = readdir(proc);
    while (de_qemu != NULL)
    {
        char d_qemu_name[FILENAMELEN];
        sprintf(d_qemu_name, "%s/%s", PROC_DIR_NAME, de_qemu->d_name);
        pid_t cur_qemu_pid = strtoll(de_qemu->d_name, NULL, 0);

		if ((opt_qemu_pid ==0) || (opt_qemu_pid == cur_qemu_pid)) {

			// Open /proc directory for traversal.
    		proc_guest = opendir(d_qemu_name);
    		if (proc == NULL) {
        		ERR("Unable to open directory \"%s\" for traversal (errno=%d). (4)\n", d_qemu_name, errno);
        		retval = -1;
        		goto done;
    		}
    		
			de_guest = readdir(proc_guest);
    		while (de_guest != NULL){

        		char d_guest_name[FILENAMELEN];
        		sprintf(d_guest_name, "%s/%s", d_qemu_name, de_guest->d_name);
        		pid_t cur_guest_pid = strtoll(de_guest->d_name, NULL, 0);

				if ((opt_guest_pid ==0) || (opt_guest_pid == cur_guest_pid)) {

					// ...if the entry is a numerically-named directory...
					if (is_directory(d_guest_name) &&  is_wholly_numeric(de_guest->d_name))
					{
						char m_name[FILENAMELEN];
						char pm_name[FILENAMELEN];
						char line[LINELEN];

						//Open pid/maps file for reading.
						sprintf(m_name, "%s/%s/%s/%s", PROC_DIR_NAME, de_qemu->d_name, de_guest->d_name, MAPS_NAME);
						m = fopen(m_name, "r");
						if (m == NULL)
						{
							ERR("Unable to open \"%s\" for reading (errno=%d) (5).\n", m_name, errno);
							continue;
						}

						// Open pid/pagemap file for reading.
						sprintf(pm_name, "%s/%s/%s/%s", PROC_DIR_NAME, de_qemu->d_name, de_guest->d_name, PAGEMAP_NAME);
						pm = open(pm_name, O_RDONLY);
						if (pm == -1)
						{
							ERR("Unable to open \"%s\" for reading (errno=%d). (7)\n", pm_name, errno);
							retval = -1;
							goto done;
						}


						uint64_t current_Virtual_TLB_contiguity = 1;
						uint64_t current_Range_TLB_contiguity = 1;
						uint64_t current_Guest_Range_TLB_contiguity = 1;


						// START implementation
						// For each line in the maps file...
						while (fgets(line, LINELEN, m) != NULL)
						{
							unsigned long vm_start; // beginning of the current vma
							unsigned long vm_end;   // end of the current vma
							int num_pages = 0;      // size of the current vma

							unsigned long vpn;      // current vpn that is inspected
							unsigned long long gfn; // and corresponding guest pfn
							unsigned long long pfn; // and corresponding host pfn (the actual machine physical address)
							unsigned long gflags;
                            unsigned long hflags;


							unsigned long prev_vpn = 0;         // previous vpn
							unsigned long long prev_gfn = 0;    // previous gfn
							unsigned long long prev_pfn = 0;    // previous pfn
							unsigned long prev_gflags;
                            unsigned long prev_hflags;

							unsigned long anchor_start_vpn, anchor_end_vpn;      // for anchor tlb

							// get the range of the vma
							n = sscanf(line, "%lX-%lX", &vm_start, &vm_end);
							if (n != 2) {
								ERR("Invalid line read from \"%s\": %s (6)\n", m_name, line);
								continue;
							}

							num_pages = (vm_end - vm_start) / PAGE_SIZE;
							vpn = vm_start / PAGE_SIZE;
							anchor_start_vpn = vpn;
							// VMA TLB
							VMA_TLB_hist[num_pages]++;
							
							off64_t index = vpn;
							off64_t o;
							ssize_t t;
									
							// For each page in the vitual address range...
							while (num_pages > 0)
							{
								// contains the physical address read in /proc/$pid/pagemap
                                char buf[128];
								unsigned long current_page_size;

							
								// Seek to the appropriate index of pagemap file.
								o = lseek64(pm, index*PAGE_SIZE, SEEK_SET);
								if (o != (index*PAGE_SIZE)) {
									ERR("Error seeking to %ld in file \"%s\" (errno=%d). (8)\n", index, pm_name, errno);
									continue;
								}

								// Read a 64-bit word from each of the pagemap file...
                                t = read(pm, &buf, sizeof(*buf));
								if (t < 0) {
									ERR("Error reading file \"%s\" (errno=%d). (11)\n", pm_name, errno);
									goto do_continue;
								}

                                unsigned long glevel;
                                unsigned long hlevel;

                                sscanf(buf, "GFN:0x%llx\tGFLAGS:0x%llx\tGLEVEL:0x%x\tPFN:0x%llx\tHFLAGS:0x%llx\tHLEVEL:0x%x\n", &gfn, &gflags, &glevel, &pfn, &hflags, &hlevel);

								// if the physical page is present
								if ((gfn != 0xFFFFFFFF) && (pfn !=0xFFFFFFFF)) {

									// now we should compute whether this is a 4K or a 2M entry
									if (glevel==THP_SPTE_LEVEL && hlevel==THP_SPTE_LEVEL) {
										if ((vpn % 512 == 0) && (gfn % 512 == 0) && (pfn % 512 ==0 )) {
											Regular_TLB_2M++;
											current_page_size=512;
										}
										else { 
											printf("What is going on %lu %lu\n", vpn, pfn);
											//assert(0);
										}
									}
									else {
										Regular_TLB_4K++;
										current_page_size=1;
									}


									// 1. Check the contiguity only in the virtual address space
									if (vpn == prev_vpn + 1) {
										// the contiguity continues..
										current_Virtual_TLB_contiguity+=current_page_size;
									}
									else {
										// the contiguity just started..
										Virtual_TLB_hist[current_Virtual_TLB_contiguity]++;
										current_Virtual_TLB_contiguity = current_page_size;
									}
									// 1. end

									// 2. Check the contiguity in both virtual and physical address space
									//
									// 2.1 Contiguoous GVA to GPA (or gfn) -- 1st dimension mappings
									if ((vpn == prev_vpn + 1) && (gfn == prev_gfn + 1) && (prev_gflags==gflags)) {
										// the contiguity continues..
										current_Guest_Range_TLB_contiguity+=current_page_size;
									}
									else {
										// the contiguity just started..
										Guest_Range_TLB_hist[current_Guest_Range_TLB_contiguity]++;
										current_Guest_Range_TLB_contiguity = current_page_size;
									}

									// 2.1 Contiguoous GVA to GPA to HPA (or pfn)  -- effective 2D mappings (end-to-end (1st and 2nd dimension) contiguity that can be cached in special TLBs)
									if ((vpn == prev_vpn + 1) && (gfn == prev_gfn + 1) && (pfn == prev_pfn + 1) && (prev_gflags==gflags) && (prev_hflags==hflags)) {
										// the contiguity continues..
										current_Range_TLB_contiguity+=current_page_size;
									}
									else {
										// new contiguity just started..

										// 3. 2D Range TLB
										Range_TLB_hist[current_Range_TLB_contiguity]++;
										
										// 4. 2D Anchor TLB (rough estimation of alignment impact, optimistic)
										update_anchor_tlb(Anchor_TLB_hist, Guest_Range_TLB_hist, anchor_start_vpn, anchor_start_vpn+current_Range_TLB_contiguity);	
skip_anchor:
										// 5. start of new contiguity
										current_Range_TLB_contiguity = current_page_size;
										anchor_start_vpn = vpn;
										//5. end
									}

									// 6. track the number of mappings with same GVA-to-HPA (2D Offset) 
									Offsets[vpn-pfn]+=current_page_size;
									// 6. end

									prev_vpn = vpn + current_page_size -1;
									prev_pfn = pfn + current_page_size -1;
									prev_gfn = gfn + current_page_size -1;
									prev_gflags = gflags;
									prev_hflags = hflags;
									total_present_pages+=current_page_size;
								}

								// 7. proceed with the next page in the virtual address range..
								num_pages-=current_page_size;
								vpn+=current_page_size;
								index = vpn;
							}

							do_continue:
							;
						}
						// END implementation


						if (pm != -1)
						{
							close(pm);
							pm = -1;
						}
					}
					if (m != NULL)
					{
						fclose(m);
						m = NULL;
					}
				}
        		de_guest = readdir(proc_guest);
			}
		}
        de_qemu = readdir(proc);
    }

  // finalization phase..
  done:

    //create SpOT histogram base on offsets
    for (auto it=Offsets.begin(); it!=Offsets.end(); ++it) {
		SpOT_hist[it->second]++;
    }

    print_range_hist(out, VMA_TLB_hist, total_present_pages, "VMA_TLB");
    print_range_hist(out, Virtual_TLB_hist, total_present_pages, "Virtual_TLB");
    print_range_hist(out, Guest_Range_TLB_hist, total_present_pages, "Guest_Range_TLB");
    print_range_hist(out, Anchor_TLB_hist, total_present_pages, "Anchor_TLB");
    print_range_hist(out, Range_TLB_hist, total_present_pages, "Range_TLB");
    print_range_hist(out, SpOT_hist, total_present_pages, "SpOT");

    fprintf(out, "\n----------\n");
    fprintf(out, "working_set\n");
    fprintf(out, "----------\n");
    fprintf(out, "total_present_pages: %ld\n", total_present_pages);
    fprintf(out, "4K pages: %ld\n", Regular_TLB_4K);
    fprintf(out, "2M pages: %ld\n", Regular_TLB_2M);
    fprintf(out, "total_present_working_set: %ld (MB)\n", total_present_pages * PAGE_SIZE / 1024 / 1024);

    // Finilizing things...
    if (proc_guest != NULL) {
        closedir(proc_guest);
    }
    if (proc != NULL) {
        closedir(proc);
    }
    if (pm != -1) {
        close(pm);
    }
    if (m != NULL) {
        fclose(m);
    }
    if (out != NULL) {
        fclose(out);
    }

    return retval;
}
