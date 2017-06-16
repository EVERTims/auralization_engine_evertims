Assess Ambisonic encoding / decoding lib used in EVERTims auralization tool. Compared libraries:

* Ambix (CPP, used in EVERTims JUCE auralization engine)
* Polarch (Matlab, used to create Ambisonic to binaural decoding filters)

Against reference library:

* SPAT (IRCAM, MaxMSP)

Final comparison done in Matlab. 
Polarch gains generated during final comparison.
SPAT gains generated from MaxMSP SPAT object, exported in .txt file for comp. in Matlab
Ambix gains generated from CPP project (see "test_project_sources" directory).