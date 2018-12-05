
### What is this repository for? ###

* Parallel implementation of Edge Sampling + Induction of streaming graphs (single pass)

### How do I get set up? ###

* To compile, run make
* To run with a binary CSR file as input, type the command: \\
    ./pies -bin **filename** -f **sampling fraction** -t **number of threads** -ns "number of edge cleaning synchs" -o **output file**
* To run with a text file (edge list), type the command:\\
    ./pies -dataset **filename** -f **sampling fraction** -t **number of threads** -ns "number of edge cleaning synchs" -o **output file**
* To measure the number of edges at any point of time, append the -m flag

### Who do I talk to? ###

* Kartik Lakhotia (klakhoti@usc.edu)
* Aditya Gaur (adityaga@usc.edu)
