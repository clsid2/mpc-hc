                            Welcome to OpenCORE

                          http://www.opencore.net/

OpenCORE is the multimedia framework of Android originally contributed by
PacketVideo.  It provides an extensible framework for multimedia rendering and
authoring and video telephony (3G-324M).

The following an overview of the directory structure which includes a list of
the top-level directories along with a brief note describing the contents.

__
  |-- android  [Contains the components the interface OpenCORE with 
  |             other parts of Android]   
  |-- baselibs [Contains basic libraries for data containers, MIME string
  |             handling, messaging across thread boundaries, etc]
  |-- build_config [Contains top-level build files used to build the libraries
  |                 outside of Android]
  |-- codecs_v2 [Contains the implementations of PV's audio and video 
  |              codecs as well as the OpenMax IL interface layer]
  |-- doc       [Contains the documentation required to interface with
  |              OpenCORE]
  |-- engines   [Contains the implementation of the player and author 
  |              engines as well as a utility for metadata]
  |-- extern_libs_v2 [Contains 3rd-party libraries used by OpenCORE. 
  |                   Currently this directory contains header files 
  |                   defining the Khronos OpenMax IL interface]
  |-- extern_tools_v2 [Contains 3rd-party tools used to build OpenCORE
  |                    indpendently of the Android build system]
  |-- fileformats  [Contains the libraries for parsing a variety of
  |                 fileformats including mp4/3gp,mp3,wav,aac]
  |-- modules [Contains build files for aggregating low-level libraries]
  |-- nodes     [Contains the OpenCORE framework "nodes", which is 
  |              the abstraction used to implement independent multimedia 
  |              processing units that can be connected in a flow graph]
  |-- oscl      [This is the Operating System Compatibility Layer which 
  |              provides the mapping OS APIs as well as some basic 
  |              data structures and utilities]
  |-- protocols [Contains parsers and composers for a variety of network
  |              protocols such as HTTP, RTP/RTCP, RTSP, and SDP]
  |-- pvmi     [Contains fundamental definitions that make up OpenCORE.
  |             The directory name is an abbreviation of PacketVideo 
  |             Multimedia Infrastructure]
  |-- tools_v2  [Contains tools used to build the libraries outside of Android]

Within each library, the following directory structure, with a few exceptions,
is implemented to organize the files:

__
  |-- build
    |-- make    <- makefile to build outside of Android is here       
  |-- doc       <- directory for any documentation specific to this lib
  |-- include   <- header files that are part of the external interface go here
  |-- src       <- source and internal header files of the library
  |-- test      <- test code (follows a similar structure)
    |-- build
      |-- make
    |-- include
    |-- src

