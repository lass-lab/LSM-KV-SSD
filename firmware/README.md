# _BandSlim_ Firmware (Key-Value controller)

This directory contains the source code for the firmware of LSM-tree Key-Value Solid State Drive (KV-SSD), specifically, _BandSlim_ (ICPP '24 submission). </br>
Below is a brief overview of the major directories and their contents. </br>

## Directory Overview

### `./src`
This directory houses all the firmware source code necessary for the LSM KV-SSD's operation. </br> 
The codebase is organized into several subdirectories, each focusing on different components of the firmware. </br>

#### `./src/nvme`
This subdirectory contains the source code for the NVM Express (NVMe) controller. </br>
- The code here is currently under reorganization to improve structure and readability. 
- This part of the codebase deals with the communication protocols and processing specific to NVMe block and key-value operations.

#### `./src/sstable`
Located within this subdirectory are the implementations related to the in-device LSM-tree. </br> 
Specifically, this focuses on the Sorted Strings Table (SSTable) components. </br>
- Note that the code in this directory is also undergoing reorganization to enhance maintainability and performance.

#### `./src/memtable`
This subdirectory focuses on the MemTable components of the in-device LSM-tree. </br>
- It includes implementations crucial for the in-memory storage structures, which temporarily store data before it is written to the SSTables. 
- Similar to other parts, this section is also in the process of being cleaned and restructured.

## Additional Information

The codebase is continually evolving, with ongoing efforts to refine the code and enhance the system's robustness. Contributions and suggestions for improvements are welcome and greatly appreciated.

<!--Please ensure to follow the coding standards and guidelines provided in the `CONTRIBUTING.md` document for maintaining consistency across the codebase.-->

