# moovoo
A molecular playground for millions of atoms.

About
=====

Moovoo is a Vulkan and C++ molecule editor that give you millions of ray-traced atoms and
connections, ultra fast load times. It is still early days yet and we are seeking sponsors
to continue the development.

On the schedule is:

    Web-based thin client editing for VR and mobile.
    A scripting system similar to Pymol.
    Ability to stretch and pull chains.
    Tools for folding RNA, DNA and protiens.
    Tools for docking RNA, DNA and protiens.
    Real-time movie generation.
    Visualisation of contact maps.

Everything in Moovoo is real-time given a decent graphics card and plenty of RAM.
Vulkan supports multithreaded rendering, GPU compute, high performance mobile
rendering for VR. It supports PDB and CIF files so far, but more can be added
with an emphasis on loading speed. For example the file 5dge.cif which has 413k
atoms and 516k lines takes less than a second to load in release builds and renders
at 30fps.

The application is a single module C++ build which should take less than a couple
of seconds and has all its own dependencies and so should build out of the box
given a Vulkan SDK install from LunarG.

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
