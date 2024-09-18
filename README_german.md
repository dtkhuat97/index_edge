# libcgraph

**Autor:** FR, EA

libcgraph ist eine C-Bibliothek zum Komprimieren und Durchsuchen von RDF-Graphen.

Diese Bibliothek sollte auf Linux und Mac funktionieren, sie wird nicht auf Windows unterstützt.

Fragen bezüglich dieser Implementierung können an adlerenno gestellt werden.

## Dependencies

- [libdivsufsort](https://github.com/y-256/libdivsufsort) zum Erstellen des Suffix-Arrays
    - brew tap brewsci/bio
    - brew install Formula/libdivsufsort
    
- [serd](https://github.com/drobilla/serd) für das Command-Line-Tool zum Lesen von RDF-Graphen
    - brew install serd

- Auf manchen System kann es notwendig sein, die CMakeLists Datei anzupassen.
  - set(INCLUDES
  - ${CMAKE_CURRENT_BINARY_DIR}
  - src/bits
  - ...
  - /usr/local/include <- Pfad zum Ordner mit divsufsort64.h
  - /usr/local/include/serd-0/serd <- Pfad zum Ordner mit serd-0.h
  - )
  - target_link_libraries(${PROJECT_NAME} PRIVATE /usr/local/lib/libdivsufsort64.dylib) # link with libdivsufsort to create the suffix array  <- Pfad hier ändern
  - target_link_libraries(cgraph-cli PRIVATE /usr/local/lib/libserd-0.dylib) # RDF-parser <- Pfad hier ändern

## Build

Für das Build wird `cmake` benötigt und kann folgendermaßen kompiliert werden:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DOPTIMIZE_FOR_NATIVE=on -DWITH_RRR=on ..
make
```

Es existieren folgende Parameter für CMake:

- `-DCMAKE_BUILD_TYPE=Release` aktiviert Compiler-Optimierungen
- `-DOPTIMIZE_FOR_NATIVE=on` aktiviert prozessorspezifische Instruktionen, sodass z.B. optimierte Instruktionen für `popcnt` aktiviert werden
- `-DNO_MMAP=on` aktiviert das Lesen der Datei des komprimierten Graphen mit `read`-Systemaufrufen inkl. eines Caches anstelle von `mmap`
- `-DWITH_RRR=on` aktiviert die Unterstüzung für Bitsequenzen vom Typ RRR (siehe unten) 
- `-DCLI=off` deaktiviert das Erstellen des Command-Line-Tools

Die Bibliothek befindet sich anschließend im build-Ordner als "libcgraph.1.0.0.dylib" (macOS) oder "libcgraph.so.1.0.0" (Linux).
Weiterhin befindet sich das Command-Line-Tool im build-Ordner unter den Namen "cgraph-cli".

### Bitsequenzen vom Typ RRR

Um Bitsequenzen vom Typ RRR (siehe Bachelorarbeit) zu verwenden, kann zusätzlich das Argument `-DWITH_RRR=on` angegeben werden.
Da einige statische Tabellen, die für diese Bitsequenzen benötigt werden, mit kompiliert werden, vergrößert sich die Größe der Bibliothek um ungefähr 80 %.
Aus diesem Grund ist die Unterstützung dieser Bitsequenzen optional.

## Command-Line-Tool

Mithilfe des Command-Line-Tools "cgraph-cli" lassen sich RDF-Graphen komprimieren und durchsuchen.
Wie dieses Programm verwendet wird, wird mit `./cgraph-cli --help` ausgegeben:

```
Usage: cgraph-cli
    -h,--help                       show this help

 * to compress a RDF graph:
   cgraph-cli [options] [input] [output]
                       [input]          input file of the RDF graph
                       [output]         output file of the compressed graph

   optional options:
    -f,--format        [format]         format of the RDF graph; keep empty to auto detect the format
                                        possible values: "turtle", "ntriples", "nquads", "trig", "hyperedge"
       --overwrite                      overwrite if the output file exists
    -v,--verbose                        print advanced information

   options to influence the resulting size and the runtime to browse the graph (optional):
       --max-rank      [rank]           maximum rank of edges, set to 0 to remove limit (default: 12)
       --monograms                      enable the replacement of monograms
       --factor        [factor]         number of blocks of a bit sequence that are grouped into a superblock (default: 8)
       --sampling      [sampling]       sampling value of the dictionary; a value of 0 disables sampling (default: 32)
       --no-rle                         disable run-length encoding
       --no-table                       do not add an extra table to speed up the decompression of the neighborhood for an specific label

 * to read a compressed RDF graph:
   cgraph-cli [options] [input] [commands...]
                       [input]          input file of the compressed RDF graph

   optional options:
    -f,--format        [format]         default format for the RDF graph at the command `--decompress`
                                        possible values: "turtle", "ntriples", "nquads", "trig", "hyperedge"
       --overwrite                      overwrite if the output file exists, used with `--decompress`

   commands to read the compressed path:
       --decompress    [RDF graph]      decompresses the given compressed RDF graph
       --extract-node  [node-id]        extracts the node label of the given node id
       --extract-edge  [edge-id]        extracts the edge label of the given edge id
       --locate-node   [text]           determines the node id of the node with the given node label
       --locate-edge   [text]           determines the edge label id for the given text
       --locatep-node  [text]           determines the node ids that have labels starting with the given text
       --search-node   [text]           determines the node ids with labels containing the given text
       --hyperedges    [rank,label]*{,node}
                                        determines the edges with given rank. You can specify any number of nodes
                                        that will be checked the edge is connected to. The incidence-type is given 
                                        implicitly. The label must not be set, use ? otherwise. For example:
                                        - "4,2,?,3,?,4": determines all rank 4 edges with label 2 that are connected
                                           to the node 3 with connection-type 2 and node 4 with connection-type 4.
                                        - "2,?,?,5": determines all rank 2 edges any label that are connected
                                          to the node 5 with connection-type 1. In the sense of regular edges, 
                                          this asks for all incoming edges of node 5.
                                        Note that it is not allowed to pass no label and no nodes to this function.
                                        Use --decompress in this case.
       --node-count                     returns the number of nodes in the graph
       --edge-labels                    returns the number of different edge labels in the graph\n"
```

Das Command-Line-Tool kann über die Serd-Bibliothekt die Formate Turtle, TriG, NTriples und NQuads. Zudem wird ein  Parser für Hypergraphen unterstützt. Dieser liest eine Kante pro Zeile und teilt an jedem Leerzeichen oder Tab.
Im Hypergraph-Format wird das erste Wort wird als Label verstanden, danach sequenziell als Knoten.

## Bibliothek

Es ist möglich, Incidence-Type-RePair als Bibliothekt einzubinden. Im Ordner include liegt die entsprechende Headerdatei für die erzeugte Bibliothek.
