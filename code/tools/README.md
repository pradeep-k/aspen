# gbench
Simple Streaming Graph benchmarking

## Vision

We started the G-Bench project in 2020 for the purpose of improving standardization within streaming graph benchmarking.

## Building

### Requirements
- g++
- Cilk

## Running

### Requirements
You need to have the following files prior to running G-Bench:
- Binary file(s) denoting graph edge additions and deletions. We use binary files to best simulate fast streaming data sources. Every 8 bytes represents an:
    - edge addition: 4 byte source vertex ID followed by 4 byte destination vertex ID
    - edge deletion: 4 byte source vertex ID followed by 4 byte negative destination vertex ID
- A txt file containing the keyword AdjacencyGraph at the top to specify the format. 
    - The second line in the file should be an integer representing the total numner of vertices.
Example Adjacency Graph format:
 ```txt
 AdjacencyGraph
 2097152 
 0
 ```

### Compile G-Bench 

```shell
cd code/tools
make
```

### Arguments
G-Bench takes the following arguments:
- `-source` : 1 for binary input
- `-batch-size` : 2^(batch-size) edges processed per batch
- `-update-dir` : path to folder containing binary files containing edge additions
- `thread-count` : number of threads to use during execution
- `-f` : path to file containing adjacency graph placeholder

### Example
``` shell
./gbench -source 1 -batch-size 12 -update-dir /path/to/folder/with/binary/files/ -thread-count 19 -f /path/to/file/with/adjacency_graph.txt
```

## Credits
https://github.com/pdclab/graphbolt.git provided the basis for the kickstarter BFS implementation
