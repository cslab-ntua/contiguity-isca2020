# Contiguity-Aware Paging

This is the source-code for Contiguity Aware Paging.
An add-on on Linux memory manager to allocate target pages
and create larger-than-a-page contiguous mappings.
These mappings can be used by novel Address Translation Hardware 
to speedup translation.
CA paging is presented on
[Enhancing and Exploiting Contiguity for Fast Memory Virtualization](https://www.cslab.ece.ntua.gr/~xalverti/papers/isca20_enhancing_and_exploiting_contiguity.pdf) (ISCA '20).

## Linux configuration
Currently CA paging is optimized to work on top of Transparent Huge Pages.<br/>
Therefore, for better performance configure Linux with THP always on.
```
CONFIG_TRANSPARENT_HUGEPAGE=y
CONFIG_TRANSPARENT_HUGEPAGE_ALWAYS=y
```

To use the VM Introspection tool (VMI) and collect statistics (see below)<br/> 
Kernel Address Space Layout Randomization (KASLR) must be turned off <br/>
both for the guest and the host OS.
```
CONFIG_RANDOMIZE_BASE=n
```

There is a working .config file on this repo.

## Native Execution

### Running
CA paging is turned on for a process 
(process's page faults and memory mapped files will trigger contiguous allocations)
with a system call (NR 337).

There is a utility C-program provided to utilize CA paging easily.<br/>
Go to the directory **tools/CAPaging_scripts/run/native/**:

Compile the code:
```
gcc -o capaging capaging.c
```

Run a process with CA paging on:
```
./capaging command <command to run>
(e.g. ./capaging command ./XSBench -t 1 -s XL -l 64 -G unionized)
```

You can also run a process with CA paging by including in its source code <br/>
the following line of code:
```
syscall(337, NULL, 0, 0)
```

### Collecting Statistics
There is a utility C-program provided to collect statistics for 
the contiguity of a process's mappings (regardless using CA paging)
based on [page-collect.c](https://www.eqware.net/articles/CapturingProcessMemoryUsageUnderLinux/page-collect.c.html).<br/>
Go to the directory **tools/CAPaging_scripts/collect-statistic/native/**:<br/>

Compile the code:
```
make
```

Collect statistics for a process:
```
./page-collect -p <pid of the process> -o <output file name>
```

Output:

The output file contains contiguity statistics for the following types of mappings:
- VMA TLB: each entry is a process's VMA area.
- Virtual TLB: each entry is a set of contiguous virtual pages that are backed by physical memory. 
- Anchor TLB: each entry is a set of contiguous virtual pages mapped to contiguous physical pages, where the starting virtual address is aligned to an anchor size as described in [Hybrid TLB Coalescing](https://iamchanghyunpark.github.io/papers/htc-isca2017.pdf).
- Range TLB: each entry is a set of contiguous virtual pages mapped to contiguous physical pages with no alignment restrictions as described in [Redundant Memory Mappings](http://www.cslab.ece.ntua.gr/~vkarakos/papers/isca15_redundant_memory_mappings.pdf). **As there are no restrictions, this captures all the contiguous virtual-to-physical mappings of the process intact.**
- SpOT: each entry is a set of page mappings whith the same Offset (virtual-physical address), without the requirement that the virtual addresses are contiguous as described in [Enhancing and Exploiting Contiguity](https://www.cslab.ece.ntua.gr/~xalverti/papers/isca20_enhancing_and_exploiting_contiguity.pdf).

<details>
<summary> An output example </summary>

```
----------
VMA_TLB
----------
total_VMA_TLB_entries: 35
total_VMA_TLB_coverage: 120018 (MB)

32 entries coverage: 100.01% (30724672 4K pages)
64 entries coverage: 0.00% (0 4K pages)
128 entries coverage: 0.00% (0 4K pages)
256 entries coverage: 0.00% (0 4K pages)

Number of entries for at least 80% coverage: 1 (exact coverage 100.00%)
Number of entries for at least 90% coverage: 1 (exact coverage 100.00%)
Number of entries for at least 99% coverage: 1 (exact coverage 100.00%)

Virtual_TLB
total_Virtual_TLB_entries: 41
total_Virtual_TLB_coverage: 120006 (MB)

32 entries coverage: 100.00% (30721560 4K pages)
64 entries coverage: 0.00% (0 4K pages)
128 entries coverage: 0.00% (0 4K pages)
256 entries coverage: 0.00% (0 4K pages)

Number of entries for at least 80% coverage: 1 (exact coverage 100.00%)
Number of entries for at least 90% coverage: 1 (exact coverage 100.00%)
Number of entries for at least 99% coverage: 1 (exact coverage 100.00%)

----------
Anchor_TLB
----------
total_Anchor_TLB_entries: 3875
total_Anchor_TLB_coverage: 120006 (MB)

32 entries coverage: 13.65% (4194304 4K pages)
64 entries coverage: 27.31% (8388608 4K pages)
128 entries coverage: 53.88% (16551936 4K pages)
256 entries coverage: 81.18% (24940544 4K pages)

Number of entries for at least 80% coverage: 251 (exact coverage 80.12%)
Number of entries for at least 90% coverage: 298 (exact coverage 90.14%)
Number of entries for at least 99% coverage: 1060 (exact coverage 99.00%)

----------
Range_TLB
----------
total_Range_TLB_entries: 532
total_Range_TLB_coverage: 120006 (MB)

32 entries coverage: 100.00% (30720556 4K pages)
64 entries coverage: 100.00% (30721070 4K pages)
128 entries coverage: 100.00% (30721165 4K pages)
256 entries coverage: 100.00% (30721293 4K pages)

Number of entries for at least 80% coverage: 8 (exact coverage 80.37%)
Number of entries for at least 90% coverage: 11 (exact coverage 90.08%)
Number of entries for at least 99% coverage: 17 (exact coverage 99.39%)

----------
SpOT
----------
total_SpOT_entries: 518
total_SpOT_coverage: 120006 (MB)

32 entries coverage: 100.00% (30720707 4K pages)
64 entries coverage: 100.00% (30721095 4K pages)
128 entries coverage: 100.00% (30721179 4K pages)
256 entries coverage: 100.00% (30721307 4K pages)

Number of entries for at least 80% coverage: 8 (exact coverage 80.43%)
Number of entries for at least 90% coverage: 11 (exact coverage 90.13%)
Number of entries for at least 99% coverage: 16 (exact coverage 99.44%)

----------
working_set
----------
total_present_pages: 30721569
4K pages: 2081
2M pages: 59999
total_present_working_set: 120006 (MB)
```

</details>

## Virtualized Execution

To create guest virtual to host physical (gVA-to-hPA) contiguous mappings CA paging
must be turned on for:
- guest OS: the guest application (creates contiguous gVA-to-gPA mappings)
- host OS: the hypervisor (creates contiguous gPA-to-hPA mappings)

Example:
- In the **host** launch qemu with CA paging:
```
./capaging command qemu-system-x86_64 -enable-kvm -m 256G disk.img
```

- In the **guest** run the application with CA paging:
```
./capaging command ./XSBench -t 1 -s XL -l 64 -G unionized
```

### VMI tool
To enable statistics collection for a guest process's gVA-to-hPA mappings
from the host OS we have developped a VM Introspection tool.
It exposes in the host OS a proc interface with functionality 
similar to /proc/maps and /proc/pagemap but for a target guest process.

Specifically in the **host OS**:
- /proc/VMI/\<qemu pid\>/\<guest process pid\>/comm: prints the guest process command name
- /proc/VMI/\<qemu pid\>/\<guest process pid\>/maps: prints the guest process contiguous virtual memory regions (gVA)
- /proc/VMI/\<qemu pid\>/\<guest process pid\>/pagemap: is an interface that enables getting the hPA translation for a process's gVA. 

There are utility C-programs provided to utilize the VMI.
**Both the guest and the host must run with the same kernel and the KASLR off.**
```
CONFIG_RANDOMIZE_BASE=n
```

#### Running
Go to the directory **tools/CAPaging_scripts/run/virtual/**: <br/>
To register a guest process for VMI monitoring run **inside the guest**:

**CA paging off** (default demand paging):
```
gcc -o vmi vmi.c
./vmi command <command to run>
(e.g. ./vmi command ./XSBench -t 1 -s XL -l 64 -G unionized)
```

**CA paging on**:
```
gcc -o vmi-capaging vmi-capaging.c
./vmi-capaging command <command to run>
(e.g. ./vmi-capaging command ./XSBench -t 1 -s XL -l 64 -G unionized)
```

#### Collecting Statistics 
Go to the directory **tools/CAPaging_scripts/collect-statistics/VMI/**:<br/>
Run **in the host**:
```
./page-collect -p <pid of the guest process> -q <qemu pid> -o <output file name>
```

The output is the same as explained in the native execution section.

## Proc Interface
- /proc/capaging\_contiguity\_map: the current contiguity map of the system
- /proc/capaging/
  - failure: number and reason of failed target page allocations 
  - success: number and category of succesfull target page allocations
  - total\_alloc: total number of capaging target allocation attempts
