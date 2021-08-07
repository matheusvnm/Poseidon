# Poseidon

Poseidon is an easy to use OpenMP framework that is completely transparent to both the developer and the end-user. Without any recompilation, code modification or OS installations, it is capable of finding, automatically and at run-time, the optimal number of threads for the parallel regions and the best turbo boosting mode for all the regions (i.e., parallel and sequential) of an parallel application. It comes in two versions: 

* (I)  Poseidon Fine-grained: Strategy that optimizes the boosting mode for each region individually; 
* (II) Poseidon Coarse-grained: Strategy that optimizes the boosting mode for each region of interest (i.e., the combination of one parallel region and a sequential one);



### List of files contained in both versions of Poseidon
---

* poseidon.c            -  Poseidon functions implementation
* poseidon.h            - Poseidon header
* env.c                 -  OpenMP internal controler variables
* libgomp.h             -  libgomp header
* libgomp_g.h           -  libgomp header
* parallel.c            -  libgomp header
* Makefile.in           -  OpenMP libgomp makefile.in
* Makefile.am           -  OpenMP libgomp makefile.am



### How to install Poseidon?
---

1. Choose the version you are going to use based on your processor (Intel or AMD).
2. Choose the version of Poseidon (Fine or Coarse).
3. Copy all files into the gcc libgomp directory:
      - cp * /path/gcc-version/libgomp.
4. Compile the GCC using Make && Make install:
      - cd /path/gcc-version/
      - make
      - make install


**IMPORTANT: Poseidon only works with GCC 9.2 version or superior.**


### How to use Poseidon?
---

1. Export the library PATH:
      - export LD_LIBRARY_PATH=/path-to-gcc-bin/lib64:$LD_LIBRARY_PATH
      
2. Set Poseidon's environment variable:
      - export OMP_POSEIDON=TRUE
    
3. Execute the application.










### Acknowledgement
---

Poseidon has been mainly developed by Sandro Matheus Vila Nova Marques (sandro-matheus@hotmail.com) during his BSc. under supervision of Arthur Francisco Lorenzon (aflorenzon@unipampa.edu.br).

When using Poseidon, please use the following reference:

Marques, Sandro M., et al. "Optimizing Parallel Applications via Dynamic Concurrency Throttling and Turbo Boosting." 2021 29th Euromicro International Conference on Parallel, Distributed and Network-Based Processing (PDP). IEEE, 2021.





