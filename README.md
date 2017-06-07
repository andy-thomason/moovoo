# moovoo
A molecular playground for millions of atoms.

About
=====

Moovoo is a Vulkan and C++ molecule editor that give you millions of ray-traced atoms and
connections, ultra fast load times. 

I am currently conducting a survey of people who use structural biology tools to see what
kind of problems they solve and whether we can create better tools using modern methodologies
such as GPU compute and machine learning to play with very large structures of atoms
beyond the scale of rhibosomes such as DNA and RNA structure.

There is some basic infrastructural work to be done to provide the core functionality
of legacy tools such as PyMol.

Please send me your data if possible, especially very complex structures that are
difficult to render using existing software.

On the schedule is:

    Web-based thin client editing for VR and mobile.
    A scripting system.
    Ability to stretch and pull chains.
    Tools for folding RNA, DNA and protiens.
    Tools for docking RNA, DNA and protiens.
    Real-time movie generation.
    Visualisation of contact maps.
    Javascript, R and Python bindings.
    Direct access to the PDB databank via the REST api.

Everything in Moovoo is real-time given a decent graphics card and plenty of RAM.
Vulkan supports multithreaded rendering, GPU compute, high performance mobile
rendering for VR. It supports PDB and CIF files so far, but more can be added
with an emphasis on loading speed. For example the file 5dge.cif which has 413k
atoms and 516k lines takes less than a second to load in release builds and renders
at 30fps.

The application is a single module C++ build which should take less than a couple
of seconds and has all its own dependencies and so should build out of the box
given a Vulkan SDK install from LunarG. What is more it has comments and meaningful
variable names.

It should run on Windows, Linux, Android and Apple devices.

Building
========

To build visual studio solutions and makefile respectively:

Windows

    mkdir build
    cd build
    cmake -G "Visual Studio 14 2015 Win64" ..
    moovoo.sln

Linux

    mkdir build
    cd build
    cmake ..
    make
    
Screen shots
============

![Alt text](textures/rhibosome.jpg?raw=true "Rhibosome with >400k atoms")
