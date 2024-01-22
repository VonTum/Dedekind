# The Dedekind Project

This codebase contains all code I built over the course of the three years of the Quest to find the 9th Dedekind Number. 
It started as my 2020-2021 Master's thesis in Civil Engineering at KU Leuven, which I successfully defended in June 2021. 
After finishing the thesis I continued working on it at home because our FPGA idea seemed promising. 
On March 8th we found the 9th Dedekind Number, and published our result on April 6th right after Christian Jäkel's preprint on his computation came out. 

## Dedekind Numbers

| D(n) |  |
| --- | --- |
| D(0) | 2 |
| D(1) | 3 |
| D(2) | 6 |
| D(3) | 20 |
| D(4) | 168 |
| D(5) | 7581 |
| D(6) | 7828354 |
| D(7) | 2414682040998 |
| D(8) | 56130437228687557907788 |
| **D(9)** | **286386577668298411128469151667598498812366** |

## Components
- [dedelib](dedelib) This contains all reuseable utilities for working with MBFs and AntiChains, Parallelization utilities, and the various algorithms needed for working in this domain. 
- [indev](indev) Used to quickly test new developments, old and not really used anymore
- [tests](tests)
- [benchmarks](benchmarks)
- [production](production) This project gives a commandline interface to work with the dedelib. It is the artifact that you should build if you want to use this library. 
- [fpga_production](fpga_production) This project contains all code for the Intel Stratix 2800GX FPGA Accelerator for computing the 9th Dedekind Number. Requires Intel's OpenCL library to build, which is already being deprecated by them, so you probably won't be able to build this. 
- [hardware](hardware) This contains all Verilog and OpenCL code for the FPGA accelerator itself. If you have Intel Quartus and AOCL you may be able to build this. 

## Building and use
Building `production` does not require any external libraries other than the OS provided ones. 

Run `setup.sh` or `setupNUMA.sh` (depending on if you have a system with NUMA nodes) to set up the build/build_debug / build_numa/build_debug_numa folders. 

Then in your preferred build folder, run `cmake --build . --parallel --target production` to build it. 

Run `./production help` for a list of all commands it supports. Most commands end with a number. This specifies the number of dimensions of the basic block MBFs used in computation. Most commands have an optimized 7-dimensional version. 

You can generate the main buffers used with `./production preCompute7`. This will use some 100GB of disk space, at least 32GB of memory and takes several hours. You should make sure to create a data/ folder in the repository directory (Dedekind) first. 

## Online resources of the project
![Computation of the 9th Dedekind Number using FPGA Supercomputing (Talk at PC2)](https://www.youtube.com/watch?v=kFfmmB3irWU)

[A computation of D(9) using FPGA Supercomputing (Arxiv preprint)](https://arxiv.org/abs/2304.03039)

```BibTeX
@misc{vanhirtum2023computation,
      title={A computation of D(9) using FPGA Supercomputing}, 
      author={Lennart Van Hirtum and Patrick De Causmaecker and Jens Goemaere and Tobias Kenter and Heinrich Riebler and Michael Lass and Christian Plessl},
      year={2023},
      eprint={2304.03039},
      archivePrefix={arXiv},
      primaryClass={cs.DM}
}
```

[A path to compute the 9th Dedekind Number using FPGA Supercomputing (Lennart Van Hirtum's Master's Thesis)](https://hirtum.com/thesis.pdf)

[Data Files](https://hirtum.com/dedekind)

## Media Attention
We have been grateful to gain such an outpouring of media attention to our project. The computation of the 9th Dedekind Number has been featured in various online publications, such as 
[Quanta Magazine](https://www.quantamagazine.org/ninth-dedekind-number-found-by-two-independent-groups-20230801/),
[Scientific American](https://www.scientificamerican.com/article/mathematicians-discover-long-sought-dedekind-number/),
[New Scientist](https://www.newscientist.com/article/2380893-mathematicians-calculate-42-digit-number-after-decades-of-trying/), and 
[Phys.org](https://phys.org/news/2023-06-ninth-dedekind-scientists-long-known-problem.html). 

It has also been picked up by the Flemish Newspaper [De Standaard](https://www.standaard.be/cnt/dmf20230728_96534962) 
and the German [Neue Westfälische](https://www.nw.de/lokal/kreis_paderborn/paderborn/23599776_Paderborner-Wissenschaftler-loesen-mit-Superrechner-Noctua-mathematisches-Problem.html). 

## Authors
- Lennart Van Hirtum
- Prof. Dr. Patrick De Causmaecker
- Jens Goemaere
- Dr. Tobias Kenter
- Dr. Heinrich Riebler
- Dr. Michael Lass
- Prof. Dr. Christian Plessl

## Acknowledgements
The authors gratefully acknowledge the computing time provided to them on the high-performance computers Noctua 2 at the NHR Center PC2. These are funded by the Federal Ministry of Education and Research and the state governments participating on the basis of the resolutions of the GWK for the national highperformance computing at universities (www.nhr-verein.de/unsere-partner).
