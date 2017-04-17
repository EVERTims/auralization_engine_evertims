# EvertSE: Auralization Engine of the EVERTims project

Pre-compiled application may already exist for your architecture, check the install section of the [EVERTims website](https://evertims.github.io/website).  The installation described here targets developers involved in the project.

## Install OSX

* install dependencies: run the ./libs/build_macos.sh script
* Install [JUCE's Projucer](http://www.juce.com/)
* Install [XCode](https://developer.apple.com/xcode/)
* Open ./EvertSE.jucer in Projucer, **from there** open the Xcode project
* Compile the project in XCode

## Externals libraries and dependencies

This software uses the JUCE C++ framework, available under both the GPL License and a commercial license. More information on http://www.juce.com.

other libraries being used:
* Eigen (MPL2, http://eigen.tuxfamily.org), 
* Ambix (GPL v2, https://github.com/kronihias/ambix/blob/master/README.md)